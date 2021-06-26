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

void optParse(int argc, char* argv[]);
void optExe();

void print_usage(const char *programname) {
    printf("usage: %s -f filename -w dirname[,n=0] -W file1[,file2] -D dirname 
        -r file1[,file2] -R [n=0] -d dirname  -t time -l file1[,file2] -u file1[,file2] 
            -c file1[,file2] -p -h\n", programname);
}

queue q = qcreate();

int main(int argc, char* argv[]) {

    optParse(argc, argv);

    int err;
    struct timespec abstime;
    abstime.tv_sec = 5;
    if((err = openConnection(SOCKNAME, 100, abstime)) == -1) {
        perror("Connection failed");
        return -1;
    }

    int opcode;
    while(1) {
        fgets(str, N, stdin);

        SYSCALL(r, writen(fd_skt, opcode, sizeof(int)), "write");
        printf("Ho scritto: %s\n", int);

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

void optParse(int argc, char* argv[]) {
    char* arval = NULL;
    int opt;
    int f_flag = 0;

    while((opt = getopt(argc, argv, "f:w:W:D:r:R:d:t:l:u:c:ph")) != -1) {
        switch(opt) {
            case 'f':
                break;
            case 'w':
                break;
            case 'W':
                break;
            case 'D':
                break;
            case 'r':
                arval = optarg;
                q = enqueue(q, 'r', (void*)arval);
                break;
            case 'R':
                break;
            case 'd':
                break;
            case 't':
                break;
            case 'l':
                break;
            case 'u':
                break;
            case 'c':
                break;
            case 'p':
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default: /* '?' */
                if (optopt == 'n' || optopt == 'm' || optopt == 'o')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                break;
        }
    }

    int index;
    for (index = optind; index < argc; index++) {
        printf ("Non-option argument %s\n", argv[index]);
    }
}


void optExe() {

    int err = 0;
    char* buf = (char*) malloc(sizeof(char)*256);

    while(q->head != NULL) {
        node* curr = pop(q);

        SYSCALL_PRINT("openFile", err, openFile((char*)(curr->data), 0), "open error\n");
        SYSCALL_PRINT("readFile", err, readFile((char*)(curr->data), (void**)&buf, sizeof(buf)), "read error\n");
        SYSCALL_PRINT("closeFile", err, closeFile((char*)(curr->data)), "close error\n");

        free(curr);
    }


}