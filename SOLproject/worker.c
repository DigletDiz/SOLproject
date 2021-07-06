#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>

#include <commcs.h>
#include <util.h>
#include <conn.h>
#include <list.h>
#include <icl_hash.h>

typedef struct wargs{
	long clsock;
	long shutdown;
	long wpipe;
	icl_hash_t* fileht;
	icl_hash_t* openht;
} wargs;


//write server feedback
void serverfb(int connfd, int fb) {

	int feedback = fb;
	int err;
	err = writen((long)connfd, &feedback, sizeof(int));
    if(err == -1) {
    	perror("writen");
    	fprintf(stderr, "Error in sending feedback....\n");
		return;
	}
	return;
}


//write server feedback
void serverReply(int connfd, reply* rep, char* pathname, char* content) {

	strcpy(rep->pathname, pathname);
	strcpy(rep->content, content);
	int err;
	//writing reply to the client 
	err = writen((long)connfd, rep, sizeof(reply));
    if(err == -1) {
    	perror("writen");
    	fprintf(stderr, "Error in sending server's reply....\n");
		return;
	}

	printf("File %s read for client %d\n", pathname, connfd);

	return;
}


//used by readNfiles to send N or all files to client 
int readNfiles(icl_hash_t* ht, int connfd, reply* rep, int N) {
    icl_entry_t *bucket, *curr;
    int i;
    int filesread = 0;

    if(!ht) return -1;

    i = 0;
    while(i < ht->nbuckets && filesread < N) {
        bucket = ht->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key) {
				serverReply(connfd, rep, (char*)curr->key, (char*)curr->data);
                filesread++;
            }
            curr=curr->next;
        }
        i++;
    }

    return filesread;
}

// funzione eseguita dal Worker thread del pool
void worker(void *arg) {
    assert(arg);
    wargs* args = (wargs*)arg;
    int connfd = (int)args->clsock;
    //long* termina = (long*)(args->shutdown);
	int wep = (int)args->wpipe;
	icl_hash_t* fileht = args->fileht;
	icl_hash_t* openht = args->openht;
    free(arg);

	//icl_hash_dump(stdout, fileht);

	//malloc for the client's request
	request* req = (request*) malloc(sizeof(request));
    if(req == NULL) {
        perror("malloc failed\n");
		serverfb(connfd, ENOMEM);
        return;
    }
	//memset(req, 0, sizeof(request));

	int err = 0;

	printf("WORKER INIZIO\n");
	//reading client's request		
	if((err = readn((long)connfd, req, sizeof(request))) == -1) {
	    perror("readn");
        fprintf(stderr, "Error in reading request....\n");

		//writing back file descriptor
		if((err = writen((long)wep, &connfd, sizeof(int))) == -1) {
		    perror("pipe write\n");
		    return;
		}
	    return;
	}
	//client closed connection
	if(err == 0) {
		printf("Avvio chiusura\n");
		//removing client from hashtable
		int length = snprintf(NULL, 0, "%d", connfd);
		char* str = malloc(length + 1);
		if(str == NULL) {
        	perror("malloc failed\n");
        	return;
    	}
		snprintf(str, length + 1, "%d", connfd);

		//flist* ln = icl_hash_find(openht, (void*)str);
		//printf("%s\n", ln->head->pathname);
		//listDelete(&(ln->head));
		int check = icl_hash_delete(openht, (void*)str, free, listDestroyicl);
		if(!check) {
			printf("Rimosso client dall'hashtable con successo\n");
		}
		else {
			printf("Rimozione fallita\n");
		}
		free(req);
		free(str);
		close(connfd);
		printf("Client %d closed\n", connfd);
		return;
	}
	
	int opcode = req->code;
	printf("Operazione richiesta %d\n", opcode);

	printf("File richiesto %s\n", req->pathname);

	switch(opcode) {

		case OPENFILE:
		{
			int flags = req->flags;
			char* pathname = req->pathname;
			
			if(flags == 0) { //0 = no flags
				char* data = icl_hash_find(fileht, (void*)pathname);
				if(data == NULL) {
					printf("File %s doesn't exist\n", pathname);
					serverfb(connfd, ENOENT);
					break;
				}

				int length = snprintf(NULL, 0, "%d", connfd);
				char* str = malloc(length + 1);
				if(str == NULL) {
        			perror("malloc failed\n");
					serverfb(connfd, ENOMEM);
        			break;
    			}
				snprintf(str, length + 1, "%d", connfd);

				//inserting in the openhashtable the opened file for the client
				flist* dt = icl_hash_find(openht, (void*)str);
				if(dt == NULL) {
					printf("Open file: Client not found\n");
					serverfb(connfd, EBADR); //sending error
					break;
				}
				int search = listFind(dt->head, pathname);
				if(search == 0) { //file already opened
					printf("Open file: file already opened\n");
					serverfb(connfd, EPERM); //sending error
					break;
				}
				else {
					listInsertHead(&(dt->head), pathname);
				}

				free(str);

				printf("File %s opened for client %d\n", pathname, connfd);
				
				serverfb(connfd, 0); //sending success
			}
			else if(flags == 1) { //1 = O_CREATE
				char* data = (char*) malloc(sizeof(char)*256);
				if(data == NULL) {
        			perror("malloc failed\n");
					serverfb(connfd, ENOMEM);
        			break;
    			}
				icl_entry_t* new = icl_hash_insert(fileht, (void*)pathname, (void*)data);
				if(new == NULL) {
					printf("File %s already exists\n", pathname);
					serverfb(connfd, EEXIST); //sending error
					break;
				}
				printf("File %s inserted in the storage\n", pathname);
				//inserting in the openhashtable the opened file for the client
				int length = snprintf(NULL, 0, "%d", connfd);
				char* str = malloc(length + 1);
				if(str == NULL) {
        			perror("malloc failed\n");
					serverfb(connfd, ENOMEM);
        			break;
    			}	
				snprintf(str, length + 1, "%d", connfd);

				flist* dt = icl_hash_find(openht, (void*)str);
				if(dt == NULL) {
					printf("Open file: Client not found\n");
					serverfb(connfd, 1); //sending error
					break;
				}
				listInsertHead(&(dt->head), pathname);

				free(str);

				printf("File %s opened for client %d\n", pathname, connfd);
				serverfb(connfd, 0); //sending success
				break;
			}
			else if(flags == 2) {printf("O_LOCK not supported\n");serverfb(connfd, -1);} //O_LOCK
			else if(flags == 3) {printf("O_LOCK not supported\n");serverfb(connfd, -1);} //O_CREATE && O_LOCK
			else {printf("flags not recognized");serverfb(connfd, -1);}

			break;
		}
		case READFILE:
		{
			//Does the file exist?
			char* pathname = req->pathname;
			char* data = icl_hash_find(fileht, (void*)pathname);
			if(data == NULL) {
				printf("File %s doesn't exist\n", pathname);
				serverfb(connfd, ENOENT);
				break;
			}

			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			if(str == NULL) {
        		perror("malloc failed\n");
				serverfb(connfd, ENOMEM);
        		break;
    		}
			snprintf(str, length + 1, "%d", connfd);

			//Did the client open the file?
			flist* opfilel = icl_hash_find(openht, (void*)str);
			if(opfilel == NULL) {
				printf("Read file: Client not found\n");
				serverfb(connfd, EBADR); //sending error
				break;
			}
			int found = listFind(opfilel->head, pathname);
			if(found == -1) {
				printf("Client %d must open file %s before trying to read it\n", connfd, pathname);
				serverfb(connfd, EPERM); //sending error
				break;
			}

			free(str);

			reply* rep = (reply*) malloc(sizeof(reply));
			if(rep == NULL) {
        		perror("malloc failed\n");
				serverfb(connfd, ENOMEM);
        		break;
    		}
			memset(rep, 0, sizeof(reply));

			//richiesta accettata
			serverfb(connfd, 0);

			serverReply(connfd, rep, pathname, data);

			free(rep);
			
			break;
		}
		case READNFILES:
		{
			int N = req->nfiles;
			int toread;

			if(N <= 0 || fileht->nentries < N) {
				toread = fileht->nentries;
			}
			else {
				toread = N;
			}

			err = writen((long)connfd, &toread, sizeof(int));
    		if(err == -1) {
    			perror("writen");
    			fprintf(stderr, "Error in sending number of files to be read....\n");
				break;
			}

			reply* rep = (reply*) malloc(sizeof(reply));
			if(rep == NULL) {
        		perror("malloc failed\n");
				serverfb(connfd, errno);
        		break;
    		}
			memset(rep, 0, sizeof(reply));

			//richiesta accettata
			serverfb(connfd, 0);

			//cycles the hashtable and reads "N" files for the client
			int filesread;
			filesread = readNfiles(fileht, connfd, rep, toread);
			if(filesread == -1) {
				perror("ReadNfiles");
				serverfb(connfd, -1);
				break;
			}

			printf("ReadNFiles: Success\n");
			serverfb(connfd, filesread);

			free(rep);

			break;
		}
		case WRITEFILE:
			break;
		case APPENDTOFILE:
		{
			//Does the file exist?
			char* pathname = req->pathname;
			char* toapp = req->toappend;
			char* data = icl_hash_find(fileht, (void*)pathname);
			if(data == NULL) {
				printf("File %s doesn't exist\n", pathname);
				serverfb(connfd, ENOENT);
				break;
			}

			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			if(str == NULL) {
        		perror("malloc failed\n");
				serverfb(connfd, ENOMEM);
        		break;
    		}
			snprintf(str, length + 1, "%d", connfd);

			//Did the client open the file?
			flist* opfilel = icl_hash_find(openht, (void*)str);
			if(opfilel == NULL) {
				printf("Read file: Client not found\n");
				serverfb(connfd, EBADR); //sending error
				break;
			}
			int found = listFind(opfilel->head, pathname);
			if(found == -1) {
				printf("Client %d must open file %s before trying to append\n", connfd, pathname);
				serverfb(connfd, EPERM); //sending error
				break;
			}

			free(str);

			strcat(data, toapp);

			//request fullfilled
			serverfb(connfd, 0);
			
			break;
		}
		case LOCKFILE:
			break;
		case UNLOCKFILE:
			break;
		case CLOSEFILE:
		{
			char* pathname = req->pathname;
				
			char* data = icl_hash_find(fileht, (void*)pathname);
			if(data == NULL) {
				printf("File %s doesn't exist\n", pathname);
				serverfb(connfd, 1); //sending error
				break;
			}

			//converting connfd in str
			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			snprintf(str, length + 1, "%d", connfd);

			//closing the file for the client
			flist* dt = icl_hash_find(openht, (void*)str);
			if(dt == NULL) {
				printf("Close file: Client not found\n");
				serverfb(connfd, 1); //sending error
				break;
			}
			listRemove(&(dt->head), pathname);
			free(str);
			printf("File %s closed for client %d\n", pathname, connfd);
			
			serverfb(connfd, 0); //sending success

			break;
		}
		case REMOVEFILE:
			break;
		case CLOSECONNECTION:
			break;
		default:
			break;
	}

	printf("Richiesta conclusa\n");

	free(req);

	//rimanda il descrittore al main thread
	if((err = writen((long)wep, (void*)&connfd, sizeof(int))) == -1) {
	    perror("pipe write\n");
	    return;
	}
	    
	return;
}