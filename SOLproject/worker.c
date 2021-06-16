#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>

#include <conn.h>


// converte tutti i carattere minuscoli in maiuscoli
static void toup(char *str) {
    char *p = str;
    while(*p != '\0') { 
        *p = (islower(*p)?toupper(*p):*p); 
	++p;
    }        
}

// funzione eseguita dal Worker thread del pool
// gestisce una intera connessione di un client
//
void worker(void *arg) {
    assert(arg);
    long* args = (long*)arg;
    long connfd = args[0];
    //long* termina = (long*)(args[1]);
	int wep = args[2];
    free(arg);
    
	int* opcode = calloc(1, sizeof(int));
	if(!opcode) {
	    perror("calloc");
	    fprintf(stderr, "Memoria esaurita....\n");
	    return;
	}

	int n;		
	if((n = readn(connfd, opcode, sizeof(int))) == -1) {
	    perror("read");
	    free(str);
	    return;
	}
	if(n == 0) {
		printf("Client closed\n");
		close(connfd);
		return;
	}
	
	switch(opcode) {

		case OPENFILE:		
			break;
		case READFILE:
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

	/*printf("Ho letto: %s\n", str);
	toup(str);
	printf("Pronto a mandare: %s\n", str);
	if((n = writen(connfd, str, N * sizeof(char))) == -1) {
	    perror("write");
	    free(str);
	    return;
	}*/

	free(opcode);

	//rimanda il descrittore al main thread
	if((n = writen(wep, &connfd, sizeof(long))) == -1) {
	    perror("pipe write\n");
	    return;
	}

	printf("Done\n");
	    
	return;
}
