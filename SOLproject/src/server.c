#define _POSIX_C_SOURCE 2001112L
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>

#include <threadpool.h>
#include <conn.h>
#include <util.h>
#include <worker.h>
#include <icl_hash.h>
#include <list.h>
#include <queue.h>
#include <rwmutex.h>

#define CONFIG "server_config.txt"

typedef struct wargs{
	long clsock;
	long wpipe;
} wargs;


volatile long uscita;

//nbuckets = max file number on config
char sockname[256];
int workers;
int file_nbuckets;
int client_nbuckets;
int storage_capacity;
int memory_occupied;
int cores;
int num_bucket_per_mutex;
int mutexnum;
int currentfile;

//hashtable containing files
//and read-write mutex array for filehashtable
icl_hash_t* fileht;
rwmutex* mutexes;

//hashtable containing clients and for each client the list of the opened files
//and mutex for clienhashtable
icl_hash_t* clientht;
pthread_mutex_t clienthtmutex;

//queue of files, utilized to know which file has to be expelled
//and mutex for the filequeue
queue* filequeue;
pthread_mutex_t filemutex;

//worker function
void worker(void *arg);


int readConfig() {
    char *filename = CONFIG;
    FILE *fp = fopen(filename, "r");

    if(fp == NULL) {
        printf("Error: could not open file %s", filename);
        return -1;
    }

    char buffer[256];
	memset(buffer, 0, 256);
	char* token;
    char* rest;
	int err;

	int i = 0;
    while(fgets(buffer, 256, fp)) {

		rest = buffer;
   		token = strtok(rest, "=");
		token = strtok(NULL, "\r\n");
		
		//printf("%s\n", token);
		
		switch(i) {
			case 0:
			{
				//printf("REST %s\n", token);
				if(token != NULL) {
					strcpy(sockname, token);
				}
				else {
					fprintf(stderr, "CONFIG PARSE ERROR: SOCKNAME WILL BE SET WITH A DEFAULT VALUE %s\n", "./cs_sock");
				}
				break;
			}
			case 1:
			{
				//printf("MAX_FILE %s\n", token);

				int maxfile;
				err = isNumber(token, &maxfile);
                if(err != 0 || maxfile <= 0) {
					fprintf(stderr, "CONFIG PARSE ERROR: MAX_FILE WILL BE SET WITH A DEFAULT VALUE %d\n", 100);
				}
				else {
					file_nbuckets = maxfile;
				}
				break;
			}
			case 2:
			{
				int megabytes;
				err = isNumber(token, &megabytes);
                if(err != 0 || megabytes <= 0) {
					fprintf(stderr, "CONFIG PARSE ERROR: STORAGE_CAPACITY WILL BE SET WITH A DEFAULT VALUE %d\n", 1000000);
				}
				else {
					storage_capacity = megabytes;
				}
				break;
			}
			case 3:
			{
				int clients;
				err = isNumber(token, &clients);
                if(err != 0 || clients <= 0) {
					fprintf(stderr, "CONFIG PARSE ERROR: CLIENTS_EXPECTED WILL BE SET WITH A DEFAULT VALUE %d\n", 20);
				}
				else {
					client_nbuckets = clients;
				}
				break;
			}
			case 4:
			{
				int cor;
				err = isNumber(token, &cor);
                if(err != 0 || cor <= 0) {
					fprintf(stderr, "CONFIG PARSE ERROR: CORES WILL BE SET WITH A DEFAULT VALUE %d\n", 4);
				}
				else {
					cores = cor;
				}
				break;
			}
			case 5:
			{
				int threadw;
				err = isNumber(token, &threadw);
                if(err != 0 || threadw <= 0) {
					fprintf(stderr, "CONFIG PARSE ERROR: THREAD_WORKERS WILL BE SET WITH A DEFAULT VALUE %d\n", 5);
				}
				else {
					workers = threadw;
				}
				break;
			}
			default:
			{
				fprintf(stderr, "AH NON LO SO IO\n");
				break;
			}
		}
		memset(buffer, 0, 256);
		i++;
	}

    // close the file
    fclose(fp);

    return 0;
}


void mutexdestroy(rwmutex* mut, int size) {
	
	free(mut);

	return;
}


void exit_clean(icl_hash_t* fileht, icl_hash_t* clientht, rwmutex* mut, threadpool_t* pool, char* sockname, queue* filequeue, int force) {
	
	int extype;

	if(force == 1) {extype = 1;}
	else {extype = 0;}

	if(pool) destroyThreadPool(pool, extype);

	if(mut) mutexdestroy(mut, mutexnum);

	if(filequeue) qdestroy(filequeue);

	if(fileht) icl_hash_destroy(fileht, free, free);
	if(clientht) icl_hash_destroy(clientht, free, listDestroyicl);
    
    if(sockname) unlink(sockname);   

	return;
}


/**
 *  @struct sigHandlerArgs_t
 *  @brief struttura contenente le informazioni da passare al signal handler thread
 *
 */
typedef struct {
    sigset_t     *set;           /// set dei segnali da gestire (mascherati)
    int           signal_pipe;   /// descrittore di scrittura di una pipe senza nome
} sigHandler_t;

// funzione eseguita dal signal handler thread
static void *sigHandler(void *arg) {
    sigset_t *set = ((sigHandler_t*)arg)->set;
    int fd_pipe   = ((sigHandler_t*)arg)->signal_pipe;

    for( ;; ) {
		int sig;
		int r = sigwait(set, &sig);
		if (r != 0) {
		    errno = r;
		    perror("FATAL ERROR 'sigwait'");
		    return NULL;
		}

		switch(sig) {
			case SIGINT:
			case SIGQUIT:
				printf("ricevuto segnale %s, esco\n", (sig==SIGINT) ? "SIGINT": "SIGQUIT");
				uscita = 1;
			    close(fd_pipe);  // notifico il listener thread della ricezione del segnale
			    return NULL;
			case SIGHUP:
			    printf("ricevuto segnale %s, esco\n", "SIGHUP");
				uscita = 2;
			    close(fd_pipe);  // notifico il listener thread della ricezione del segnale
			    return NULL;
			default:  ; 
		}
    }
    return NULL;	   
}

int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i) {
    	if(FD_ISSET(i, &set)) return i;
	}
    assert(1==0);
    return -1;
}


int main(int argc, char *argv[]) {

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); 
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
    
    if (pthread_sigmask(SIG_BLOCK, &mask,NULL) != 0) {
		fprintf(stderr, "FATAL ERROR\n");   
    	return -1;
    }

    // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket
    struct sigaction s;
    memset(&s,0,sizeof(s));    
    s.sa_handler=SIG_IGN;
    if ( (sigaction(SIGPIPE,&s,NULL) ) == -1 ) {   
		perror("sigaction");  
    	return -1;
    } 

    /*
     * La pipe viene utilizzata come canale di comunicazione tra il signal handler thread ed il 
     * thread listener per notificare la terminazione. 
     */
    int signal_pipe[2];
    if (pipe(signal_pipe)==-1) {
		perror("pipe"); 
    	return -1;
    }

	/*
     * La pipe viene utilizzata come canale di comunicazione tra il worker ed il 
     * thread listener per notificare che la richiesta ?? stata servita. 
     */
	int request_pipe[2];
    if (pipe(request_pipe)==-1) {
		perror("pipe");
		return -1;
    }
    
    pthread_t sighandler_thread;
    sigHandler_t handlerArgs = { &mask, signal_pipe[1] };
   
    if (pthread_create(&sighandler_thread, NULL, sigHandler, &handlerArgs) != 0) {
		perror("errore nella creazione del signal handler thread");
		return -1;
    }

	//sockname = "./cs_sock";
	file_nbuckets = 50;
	storage_capacity = 1000000;
	client_nbuckets = 20;
	cores = 4;
	workers = 5;
	memory_occupied = 0;
	currentfile = 0;

	memset(sockname, 0, 256);

	readConfig();

	printf("MAX FILE %d\n", file_nbuckets);
	printf("STORAGE CAPACITY %d\n", storage_capacity);
	printf("WORKERS %d\n", workers);
	printf("MAX CLIENTS EXPECTED %d\n", client_nbuckets);

	if(!strlen(sockname)) {
		strcpy(sockname, SOCKNAME);
	}

	printf("%s\n", sockname);

	//ceil...
	double ceil = (double)file_nbuckets/(double)cores;
    int parteintera = (int)ceil;
    if(ceil > parteintera) {parteintera++;}

	num_bucket_per_mutex = parteintera;
	mutexnum = (file_nbuckets > cores) ? cores : file_nbuckets;

    int listenfd;
    if((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return -1;
    }

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);

    if(bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1) {
		perror("bind");	
		return -1;
    }
    if(listen(listenfd, MAXBACKLOG) == -1) {
		perror("listen");
		unlink(sockname);
		return -1;
    }

    threadpool_t *pool = NULL;

    pool = createThreadPool(workers, workers); 
    if(!pool) {
		fprintf(stderr, "ERRORE FATALE NELLA CREAZIONE DEL THREAD POOL\n");
		unlink(sockname);
		return -1;
    }
    
    fd_set set, tmpset;
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    FD_SET(listenfd, &set);        // aggiungo il listener fd al master set
    FD_SET(signal_pipe[0], &set);  // aggiungo il descrittore di lettura della signal_pipe
	FD_SET(request_pipe[0], &set);  // aggiungo il descrittore di lettura della request_pipe
    
    // tengo traccia del file descriptor con id piu' grande
    int fdmax = (listenfd > signal_pipe[0]) ? listenfd : signal_pipe[0];
	int fdre;
	int n;

	fileht = icl_hash_create(file_nbuckets, NULL, NULL);
	if(!fileht) {
		fprintf(stderr, "ERRORE FATALE NELLA CREAZIONE DELLA TABELLA HASH\n");
		exit_clean(NULL, NULL, NULL, pool, sockname, NULL, 1);
    	return -1;
    }

	filequeue = qcreate();
	pthread_mutex_init(&(filemutex), NULL);

	clientht = icl_hash_create(client_nbuckets, NULL, NULL);
	if(!clientht) {
		fprintf(stderr, "ERRORE FATALE NELLA CREAZIONE DELLA TABELLA HASH\n");
		exit_clean(fileht, NULL, NULL, pool, sockname, filequeue, 1);
    	return -1;
    }

	pthread_mutex_init(&(clienthtmutex), NULL);

	mutexes = (rwmutex*) malloc(sizeof(rwmutex) * mutexnum);
	if(mutexes == NULL) {
		perror("mutexes");
		exit_clean(fileht, clientht, NULL, pool, sockname, filequeue, 1);
    	return -1;
	}

	//inizializing mutexes
	for(int i = 0; i < mutexnum; i++) {
		rwmutex* copy = &mutexes[i];
    	pthread_mutex_init(&(copy->mutex), NULL);
		pthread_cond_init(&(copy->cond), NULL);
		copy->nreaders = 0;
		copy->waiting_writers = 0;
		copy->writing = 0;
	}


    volatile long termina=0;
    while(!termina) {
		// copio il set nella variabile temporanea per la select
		tmpset = set;
		if(select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) {
		    perror("select");
			exit_clean(fileht, clientht, mutexes, pool, sockname, filequeue, 1);
    		return -1;
		}
		// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
		for(int i=0; i <= fdmax; i++) {
		    if(FD_ISSET(i, &tmpset)) {
				int connfd;
				if(i == listenfd) { // e' una nuova richiesta di connessione 
				    if((connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL)) == -1) {
						perror("accept");
						break;
				    }

					printf("client %d connesso\n", connfd);

					int length = snprintf(NULL, 0, "%d", connfd);
					char* str = malloc(length + 1);
					snprintf(str, length + 1, "%d", connfd);

					//adding client in hashtable
					flist* hlist = (flist*) malloc(sizeof(flist));
					hlist->head = NULL;
					LOCK(&(clienthtmutex));
					icl_entry_t* prova = icl_hash_insert(clientht, (void*)str, (void*)hlist, 0);  
					if(prova == NULL) {
						//set errno
						fprintf(stderr, "IT IS IMPOSSIBLE TO INSERT CLIENT IN HASHTABLE\n");
						exit_clean(fileht, clientht, mutexes, pool, sockname, filequeue, 1);
						return -1;
					}
					//icl_hash_dump(stdout, clientht);
					else {
						FD_SET(connfd, &set);
						if(connfd > fdmax) {
							fdmax = connfd;
						}
					}
					UNLOCK(&(clienthtmutex));
				}
				else if(i == signal_pipe[0]) {
				    // ricevuto un segnale, esco ed inizio il protocollo di terminazione
				    termina = 1;
				    break;
				}
				else if(i == request_pipe[0]) {
				    //inserisco il descrittore ricevuto nel set
				    if((n = readn(request_pipe[0], &fdre, sizeof(int))) == -1) {
		    			perror("read1: fd perso");
						continue;
		    		}
					FD_SET(fdre, &set);
					if(fdre > fdmax) {
						fdmax = fdre;
					}
				}
				else { //richiesta di un client, la mando al tp
					wargs* args = (wargs*) malloc(sizeof(wargs));
				    if(!args) {
				    	perror("FATAL ERROR 'malloc'");
						exit_clean(fileht, clientht, mutexes, pool, sockname, filequeue, 1);
    					return -1;
				    }
				    args->clsock = (long)i; //client socket
					args->wpipe = (long)request_pipe[1]; //pipe write descriptor

					FD_CLR(i, &set);
					updatemax(set, fdmax);
	
				    int r = addToThreadPool(pool, worker, (void*)args);
				    if (r==0) continue; // aggiunto con successo
				    if (r<0) { // errore interno
						fprintf(stderr, "FATAL ERROR, adding to the thread pool\n");
				    } 
					else { // coda dei pendenti piena
						fprintf(stderr, "SERVER TOO BUSY\n");
						spawnThread(worker, args);
						continue;
				    }
				    free(args);
				    close(connfd);
				}
		    }
		}
    }

	exit_clean(fileht, clientht, mutexes, pool, sockname, filequeue, uscita); //l'ultimo parametro da settare a seconda del segnale

    // aspetto la terminazione del signal handler thread
    pthread_join(sighandler_thread, NULL);
 
    return 0;    
}