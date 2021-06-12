#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/select.h>

#include <conn.h>

#define N 100

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
void threadF(void *arg) {
    assert(arg);
    long* args = (long*)arg;
    long connfd = args[0];
    //long* termina = (long*)(args[1]);
	int wep = args[2];
    free(arg);
    
	char* str = calloc(N, sizeof(char));
	if(!str) {
	    perror("calloc");
	    fprintf(stderr, "Memoria esaurita....\n");
	    return;
	}

	int n;		
	if((n = readn(connfd, str, N * sizeof(char))) == -1) {
	    perror("read");
	    free(str);
	    return;
	}
	if(n == 0) {
		printf("Client chiuso\n");
		close(connfd);
		return;
	}
	printf("Ho letto: %s\n", str);
	toup(str);
	printf("Pronto a mandare: %s\n", str);
	if((n = writen(connfd, str, N * sizeof(char))) == -1) {
	    perror("write");
	    free(str);
	    return;
	}

	free(str);

	//rimanda il descrittore al main thread
	if((n = writen(wep, &connfd, sizeof(long))) == -1) {
	    perror("pipe write\n");
	    return;
	}

	printf("Servito\n");
	    
	return;
}
