#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>

#include <conn.h>

typedef struct wargs{
	long clsock;
	long shutdown;
	long wpipe;
	icl_hash_t** fileht;
	icl_hash_t** openht;
} wargs;

// funzione eseguita dal Worker thread del pool
void worker(void *arg) {
    assert(arg);
    wargs* args = (wargs*)arg;
    long connfd = args->clsock;
    //long* termina = (long*)(args->shutdown);
	int wep = (int)args->wpipe;
	icl_hash_t** fileht = args->fileht;
	icl_hash_t** openht = args->openht;
    free(arg);

	//malloc for the client's request
	request* req = (request*) malloc(sizeof(request));
    if(req == NULL) {
        perror("malloc failed\n");
        return -1;
    }

	int err = 0;

	//reading client's request		
	if((err = readn(connfd, req, sizeof(int))) == -1) {
	    perror("read");
	    return;
	}
	//client closed connection
	if(err == 0) {
		printf("Client %d closed\n", (int)connfd);
		close(connfd);
		return;
	}
	//error
	if(err == -1) {
        perror("readn");
        fprintf(stderr, "Error in reading request....\n");

		//writing back file descriptor
		if((n = writen(wep, &connfd, sizeof(long))) == -1) {
		    perror("pipe write\n");
		    return;
		}
	    return;
    }
	
	int opcode = req->code;

	switch(opcode) {

		case 1://OPENFILE
			int flags = req->flags;
			char* pathname = req->pathname;
			
			if(flags == 0) { //0 = no flags
				char* data = icl_hash_find(ht, (void*)pathname);//void * icl_hash_find(icl_hash_t *ht, void* key)
				if(data == NULL) {
					perror("File %s doesn't exist\n", pathname);
				}
				else {
					//inserting in the openhashtable the opened file for the client
					icl_hash_insert(openht, (void*)connfd, (void*)pathname);
					printf("File %s opened for client %d\n", pathname, (int)connfd);
				}
			}
			else if(flags == 1) { //1 = O_CREATE
				char* data = (char*) malloc(sizeof(char)*256);
				icl_hash_t* new = icl_hash_insert(fileht, (void*)pathname, (void*)data);
				if(new == NULL) {
					perror("File %s already exists\n", pathname);
				}
				else{
					printf("File %s inserted in the storage\n", pathname);
					//inserting in the openhashtable the opened file for the client
					icl_hash_insert(openht, (void*)connfd, (void*)pathname);
					printf("File %s opened for client %d\n", pathname, (int)connfd);
				}
			}
			else if(flags == 2) {printf("O_LOCK not supported\n");} //O_LOCK
			else if(flags == 3) {printf("O_LOCK not supported\n");} //O_CREATE & O_LOCK
			else {printf("flags not recognized");}

			break;

		case 2: //READFILE
			//Does the file exist?
			char* pathname = req->pathname;
			char* data = icl_hash_find(fileht, (void*)pathname);
			if(data == NULL) {
				perror("File %s doesn't exist\n", pathname);
				break;
			}

			//Did the client open the file?
			lnode* opfilel = icl_hash_find(openht, (void*)connfd);
			if(opfilel == NULL) {
				printf("Client not found\n");
				break;
			}
			int found = listFind(opfilel, pathname);
			if(found == -1) {
				perror("Client %d must open file %s before trying to read it\n", (int)connfd, pathname);
				break;
			}

			//writing file to the client 
			err = writen(connfd, data, sizeof(data));
    		if(err == -1) {
    			perror("writen");
    			fprintf(stderr, "Error in sending file's data....\n");
				return;
			}

			break;

		case READNFILES:
			break;
		case WRITEFILE:
			break;
		case APPENDFILE:
			break;
		case LOCKFILE:
			break;
		case UNLOCKFILE:
			break;
		case CLOSEFILE:
			break;
		case REMOVEFILE:
			break;
		default:
			break;

	}

	free(opcode);
	free(buf);

	//rimanda il descrittore al main thread
	if((n = writen(wep, &connfd, sizeof(long))) == -1) {
	    perror("pipe write\n");
	    return;
	}
	    
	return;
}