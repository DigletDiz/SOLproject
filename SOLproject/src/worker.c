#define _POSIX_C_SOURCE 2001112L
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>
#include <string.h>

#include <commcs.h>
#include <util.h>
#include <conn.h>
#include <list.h>
#include <queue.h>
#include <icl_hash.h>
#include <flags.h>
#include <rwmutex.h>

typedef struct wargs{
	long clsock;
	long wpipe;
} wargs;

extern int storage_capacity;
extern int memory_occupied;
extern int num_bucket_per_mutex;
extern int file_nbuckets;
extern int currentfile;

//hashtables
extern icl_hash_t* fileht;
extern rwmutex* mutexes;
extern icl_hash_t* clientht;
extern pthread_mutex_t clienthtmutex;

//filequeue
extern queue* filequeue;
extern pthread_mutex_t filemutex;
extern pthread_cond_t filecond;

//calculates the right mutex for the given bucket
//returns the index of mutex array
int find_right_mutex(int bucketindex) {

	if(bucketindex >= file_nbuckets) {
		errno = ERANGE;
		return -1;
	}

	int base = num_bucket_per_mutex;
	int ceiling = base;
	int mutex_index = 0;
	while(bucketindex > ceiling-1) {
		mutex_index++;
		ceiling += base;
	}

	return mutex_index;
}


void read_begin(rwmutex* mut) {

	LOCK(&(mut->mutex));
	while(mut->waiting_writers > 0 || mut->writing == 1) {
		WAIT(&(mut->cond), &(mut->mutex));
	}
	(mut->nreaders)++;
	UNLOCK(&(mut->mutex));

	return;
}


void read_end(rwmutex* mut) {

	LOCK(&(mut->mutex));
	(mut->nreaders)--;
	if(mut->nreaders == 0) {
		BCAST(&(mut->cond));
	}
	UNLOCK(&(mut->mutex));

	return;
}


void write_begin(rwmutex* mut) {

	LOCK(&(mut->mutex));
	(mut->waiting_writers)++;
	while(mut->nreaders > 0 || mut->writing == 1) {
		WAIT(&(mut->cond), &(mut->mutex));
	}
	(mut->waiting_writers)--;
	mut->writing = 1;
	UNLOCK(&(mut->mutex));

	return;
}


void write_end(rwmutex* mut) {

	LOCK(&(mut->mutex));
	mut->writing = 0;
	BCAST(&(mut->cond));
	UNLOCK(&(mut->mutex));

	return;
}


void eject_alg() {
	
	node* head;
	char* path;
	char* data;
	int err;
	size_t size;
	printf("Memory occupied %d\n", memory_occupied);
	printf("Storage capacity %d\n", storage_capacity);
	while(memory_occupied > storage_capacity) {		
		head = pop(filequeue);
		path = head->data;
		data = icl_hash_wfind(fileht, (void*)path, &size);
		if(data == NULL) {
			fprintf(stderr, "ERROR: IT IS IMPOSSIBLE TO REMOVE FILE FROM HASHTABLE, ABORT\n");
			//pthread_kill;
			return;
		}
		printf("File %s eliminated\n", path);
		err = icl_hash_delete(fileht, (void*)path, free, free);
		if(err == -1) {
			fprintf(stderr, "ERROR: IT IS IMPOSSIBLE TO REMOVE FILE FROM HASHTABLE, ABORT\n");
			//pthread_kill;
			return;
		}

		memory_occupied -= size;
		currentfile--;

		free(head);
		printf("Memory occupied %d\n", memory_occupied);
	}

	return;
}


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
void serverReply(int connfd, reply* rep, char* pathname, char* content, size_t contentsize) {

	memset(rep, 0, sizeof(reply));
	if(content == NULL) {fprintf(stderr, "Cannot read: content is NULL (probably the file was expelled)....\n");return;}
	memcpy(rep->content, content, contentsize+1);
	rep->contentsize = contentsize;
	strcpy(rep->pathname, pathname);
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
	int mutex_index;

    if(!ht) return -1;

    i = 0;
    while(i < ht->nbuckets && filesread < N) {
        bucket = ht->buckets[i];
		mutex_index = find_right_mutex(i);
        for(curr=bucket; curr!=NULL; ) {
			read_begin(&mutexes[mutex_index]);
            if(curr->key && curr->data) {
				serverReply(connfd, rep, (char*)curr->key, (char*)curr->data, curr->size);
                filesread++;
            }
            curr=curr->next;
			read_end(&mutexes[mutex_index]);
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
	int wep = (int)args->wpipe;
    free(args);

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

		LOCK(&(clienthtmutex));
		int check = icl_hash_delete(clientht, (void*)str, free, listDestroyicl);
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
		UNLOCK(&(clienthtmutex));
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
			
			if(flags == NOFLAGS) { //NOFLAGS
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

				//inserting in the clienthashtable the opened file for the client
				flist* dt = icl_hash_find(clientht, (void*)str);
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
			else if(flags == O_CREATE) { //O_CREATE

				char* data = strdup("");
				char* pathn = strdup(pathname);
				icl_entry_t* new = icl_hash_insert(fileht, (void*)pathn, (void*)data, 0);
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

				flist* dt = icl_hash_find(clientht, (void*)str);
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
			else if(flags == O_LOCK) {printf("O_LOCK not supported\n");serverfb(connfd, -1);} //O_LOCK
			else if(flags == O_CL) {printf("O_LOCK not supported\n");serverfb(connfd, -1);} //O_CREATE && O_LOCK
			else {printf("flags not recognized");serverfb(connfd, -1);}

			break;
		}
		case READFILE:
		{
			//read begin setup
			int bucket_index = hash_pjw((void*) req->pathname) % file_nbuckets;
			int mutex_index = find_right_mutex(bucket_index);
			//read begin
			read_begin(&mutexes[mutex_index]);

			//Does the file exist?
			char* pathname = req->pathname;
			size_t datasize;
			char* data = icl_hash_wfind(fileht, (void*)pathname, &datasize);
			if(data == NULL) {
				printf("File %s doesn't exist\n", pathname);
				read_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOENT);
				break;
			}

			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			if(str == NULL) {
        		perror("malloc failed\n");
				read_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
        		break;
    		}
			snprintf(str, length + 1, "%d", connfd);

			//Did the client open the file?
			flist* opfilel = icl_hash_find(clientht, (void*)str);
			if(opfilel == NULL) {
				printf("Read file: Client not found\n");
				read_end(&mutexes[mutex_index]);
				serverfb(connfd, EBADR); //sending error
				break;
			}
			int found = listFind(opfilel->head, pathname);
			if(found == -1) {
				printf("Client %d must open file %s before trying to read it\n", connfd, pathname);
				read_end(&mutexes[mutex_index]);
				serverfb(connfd, EPERM); //sending error
				break;
			}

			free(str);

			reply* rep = (reply*) malloc(sizeof(reply));
			if(rep == NULL) {
        		perror("malloc failed\n");
				read_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
        		break;
    		}

			//richiesta accettata
			serverfb(connfd, 0);

			serverReply(connfd, rep, pathname, data, datasize);

			read_end(&mutexes[mutex_index]);

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
			//write begin setup
			int bucket_index = hash_pjw((void*) req->pathname) % file_nbuckets;
			int mutex_index = find_right_mutex(bucket_index);
			//write begin
			write_begin(&mutexes[mutex_index]);

			//Does the file exist?
			char* pathname = req->pathname;
			char* toapp = req->toappend;
			int sizeapp = req->sizeappend;
			size_t oldsize;
			char* data = icl_hash_wfind(fileht, (void*)pathname, &oldsize);
			if(data == NULL) {
				printf("File %s doesn't exist\n", pathname);
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOENT);
				break;
			}

			//check if the file would be more than FILESIZE
			if(oldsize+sizeapp > FILESIZE) {
				perror("The new file would be bigger than the FILEMAXSIZE");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ECANCELED);
				break;
			}

			//check if the file would be too big
			if(oldsize+sizeapp > storage_capacity) {
				perror("The new file would be bigger than the storage capacity");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ECANCELED);
				break;
			}

			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			if(str == NULL) {
        		perror("malloc failed\n");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
        		break;
    		}
			snprintf(str, length + 1, "%d", connfd);

			//Did the client open the file?
			flist* opfilel = icl_hash_find(clientht, (void*)str);
			if(opfilel == NULL) {
				printf("Read file: Client not found\n");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, EBADR); //sending error
				break;
			}
			int found = listFind(opfilel->head, pathname);
			if(found == -1) {
				printf("Client %d must open file %s before trying to append\n", connfd, pathname);
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, EPERM); //sending error
				break;
			}

			free(str);

			//change data
			//preparing newsize
			int newsize = oldsize+sizeapp+1;

			char* newdata = (char*) malloc((sizeof(char)*newsize));
			if(newdata == NULL) {
        		perror("malloc failed");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
        		break;
    		}
			memset(newdata, 0, sizeof(newsize));

			memcpy(newdata, data, oldsize);
			memcpy(newdata+oldsize, toapp, sizeapp+1);

			icl_entry_t* curr;
    		unsigned int hash_val;

    		hash_val = (* fileht->hash_function)(pathname) % fileht->nbuckets;

			LOCK(&filemutex);
			//searching data in hashtable
			found = 0;
			curr=fileht->buckets[hash_val];
    		while((curr != NULL) && (!found)) {
        		if(fileht->hash_key_compare(curr->key, pathname)) {
            		free(curr->data);
					curr->data = newdata;
					curr->size += sizeapp;  
					found = 1;
				}
				else {
					curr=curr->next;
				}
			}
			//someone else could have expelled my file
			if(curr == NULL) {
				fprintf(stderr, "Appending file has been expelled, operation aborted\n");
				write_end(&mutexes[mutex_index]);
				UNLOCK(&filemutex);
				serverfb(connfd, ENOENT);
			}

			memory_occupied += sizeapp;

			if(memory_occupied > storage_capacity) {
				eject_alg();
			}
			UNLOCK(&filemutex);

			write_end(&mutexes[mutex_index]);

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
				serverfb(connfd, ENOENT); //sending error
				break;
			}

			//converting connfd in str
			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			if(!str) {
				perror("malloc failed");
				serverfb(connfd, ENOMEM);
        		break;
    		}
			snprintf(str, length + 1, "%d", connfd);

			//closing the file for the client
			flist* dt = icl_hash_find(clientht, (void*)str);
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
		case MWFILE:
		{
			//write begin setup
			int bucket_index = hash_pjw((void*) req->pathname) % file_nbuckets;
			int mutex_index = find_right_mutex(bucket_index);

			//write begin
			write_begin(&mutexes[mutex_index]);

			size_t fsize = req->sizecontent;

			//check if the file is too big
			if(fsize > storage_capacity) {
				perror("The new file would be bigger than the storage capacity");
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ECANCELED);
				break;
			}

			//preparing pathname and content
			char* pathname = (char*) malloc(sizeof(char)*PATHSIZE);
			if(!pathname) {
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
				break;
			}
			strncpy(pathname, req->pathname, PATHSIZE);

			char* fcontent = (char*) malloc(sizeof(char)*(fsize+1));
			if(!fcontent) {
				write_end(&mutexes[mutex_index]);
				serverfb(connfd, ENOMEM);
				break;
			}
			memcpy(fcontent, req->content, fsize+1);

			LOCK(&filemutex);

			icl_entry_t* check = icl_hash_insert(fileht, (void*)pathname, (void*)fcontent, fsize);
			if(check == NULL) {
				fprintf(stderr, "Errore insert\n");
				write_end(&mutexes[mutex_index]);
				UNLOCK(&filemutex);
				serverfb(connfd, ECANCELED);
    			break;
			}
			//updating memory occupation
			memory_occupied += fsize;
			currentfile++;
			printf("memory occupied %d\n", memory_occupied);

			//inserting new file in the queue
			enqueue(filequeue, 'w', (void*)pathname, NULL);

			if((memory_occupied > storage_capacity) || (currentfile > file_nbuckets)) {
				eject_alg();
			}

			UNLOCK(&filemutex);

			write_end(&mutexes[mutex_index]);

			//request fullfilled
			serverfb(connfd, 0);
			
			break;
		}
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