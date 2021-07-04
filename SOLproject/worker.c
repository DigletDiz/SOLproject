#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>

#include <api.h>
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
        return;
    }
	memset(req, 0, sizeof(request));

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
			char* pathname = (char*) malloc(sizeof(char)*BUFSIZE);
			strcpy(pathname, req->pathname);
			
			if(flags == 0) { //0 = no flags
				char* data = icl_hash_find(fileht, (void*)pathname);
				if(data == NULL) {
					printf("File %s doesn't exist\n", pathname);
					serverfb(connfd, 1); //sending error
					break;
				}

				int length = snprintf(NULL, 0, "%d", connfd);
				char* str = malloc(length + 1);
				snprintf(str, length + 1, "%d", connfd);

				//inserting in the openhashtable the opened file for the client
				flist* dt = icl_hash_find(openht, (void*)str);
				if(dt == NULL) {
					printf("Open file: Client not found\n");
					serverfb(connfd, 1); //sending error
					break;
				}
				listInsertHead(&(dt->head), pathname);

				//printf("Aggiunto %s\n", dt->head->pathname);

				/*//rimanda il descrittore al main thread
				if((err = writen(wep, (void*)&connfd, sizeof(int))) == -1) {
	    			perror("pipe write\n");
	    			return;
				}*/

				/*icl_entry_t* newo = icl_hash_insert(openht, (void*)connfd, (void*)pathname);
				if(newo == NULL) {
					perror("Error: insert in hashtable\n");
					serverfb(connfd, -1); //sending error
					break;
				}*/
				free(str);

				printf("File %s opened for client %d\n", pathname, connfd);
				
				serverfb(connfd, 0); //sending success
			}
			else if(flags == 1) { //1 = O_CREATE
				char* data = (char*) malloc(sizeof(char)*256);
				icl_entry_t* new = icl_hash_insert(fileht, (void*)pathname, (void*)data);
				if(new == NULL) {
					printf("File %s already exists\n", pathname);
					serverfb(connfd, 1); //sending error
					break;
				}
				printf("File %s inserted in the storage\n", pathname);
				//inserting in the openhashtable the opened file for the client
				icl_entry_t* newt = icl_hash_insert(openht, (void*)&connfd, (void*)pathname);
				if(newt == NULL) {
					perror("Error: insert in hashtable\n");
					serverfb(connfd, 1); //sending error
					break;
				}

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
				serverfb(connfd, 1); //sending error
				break;
			}

			int length = snprintf(NULL, 0, "%d", connfd);
			char* str = malloc(length + 1);
			snprintf(str, length + 1, "%d", connfd);

			//Did the client open the file?
			flist* opfilel = icl_hash_find(openht, (void*)str);
			if(opfilel == NULL) {
				printf("Read file: Client not found\n");
				serverfb(connfd, 1); //sending error
				break;
			}
			int found = listFind(opfilel->head, pathname);
			if(found == -1) {
				printf("Client %d must open file %s before trying to read it\n", connfd, pathname);
				serverfb(connfd, 1); //sending error
				break;
			}

			free(str);

			serverfb(connfd, 0); //sending success

			int datasize = strlen(data);

			//writing file's size to the client 
			err = writen((long)connfd, &datasize, sizeof(int));
    		if(err == -1) {
    			perror("writen");
    			fprintf(stderr, "Error in sending file's size....\n");
				break;
			}
			//writing file to the client 
			err = writen((long)connfd, data, datasize);
    		if(err == -1) {
    			perror("writen");
    			fprintf(stderr, "Error in sending file's data....\n");
				break;
			}

			printf("File %s read for client %d\n", pathname, connfd);

			break;
		}
		case READNFILES:
			break;
		case WRITEFILE:
			break;
		case APPENDTOFILE:
			break;
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