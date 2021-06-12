#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */

#include <conn.h>

#define SOCKNAME "./cs_sock"
#define UNIX_PATH_MAX 108
#define N 100
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e);exit(errno);}


int main(void) {
    int fd_skt, r;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
    while(connect(fd_skt, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        if(errno == ENOENT) {
            sleep(1); /* sock non esiste */
        }
        else {
            exit(EXIT_FAILURE);
        } 
    }

    char* str = (char*) calloc(N, sizeof(char));
    while(1) {
        fgets(str, N, stdin);

        SYSCALL(r, writen(fd_skt, str, sizeof(char)*N), "write");
        printf("Ho scritto: %s\n", str);

        SYSCALL(r, readn(fd_skt, str, sizeof(char)*N), "read");
        if(r == 0) {
            fprintf(stderr, "errore: read\n");
            return -1;
        }
        printf("Client got: %s\n", str);
    }

    free(str);
    SYSCALL(r, close(fd_skt), "close");
    exit(EXIT_SUCCESS); 
}