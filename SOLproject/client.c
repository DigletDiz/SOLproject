#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <time.h>

#include <conn.h>
#include <util.h>
#include <queue.h>
#include <list.h>
#include <api.h>


#define SOCKNAME "./cs_sock"
#define UNIX_PATH_MAX 108

void optParse(int argc, char* argv[]);
void optExe();

void print_usage(const char *programname) {
    printf("usage: %s -f filename -w dirname[,n=0] -W file1[,file2] -D dirname -r file1[,file2] -R [n=0] -d dirname  -t time -l file1[,file2] -u file1[,file2] -c file1[,file2] -p -h\n", programname);
}

queue* q;

int main(int argc, char* argv[]) {

    q = qcreate();

    optParse(argc, argv);
    printf("Fuori dal parse");

    int err;
    struct timespec abstime;
    abstime.tv_sec = 5;
    if((err = openConnection(SOCKNAME, 100, abstime)) == -1) {
        perror("Connection failed");
        return -1;
    }
    printf("Connected!\n");

    optExe();
    printf("Fuori dall'exe");

    return 0; 
}

void optParse(int argc, char* argv[]) {
    char* arval = NULL;
    int opt;
    //int f_flag = 0;

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
                printf("Sono in parse opt = r\n");
                arval = optarg;
                enqueue(q, 'r', (void*)arval);
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
                return;
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
    //char* buf = (char*) malloc(sizeof(char)*256);
    //size_t sis;
    node* curr;

    while(q->head != NULL) {
        printf("Sono in exe\n");
        curr = pop(q);

        SYSCALL_PRINT("openFile", err, openFile((char*)(curr->data), 0), "Open error\n", "");
        //SYSCALL_PRINT("readFile", err, readFile((char*)(curr->data), (void**)&buf, &sis), "Read error\n", "");
        //SYSCALL_PRINT("closeFile", err, closeFile((char*)(curr->data)), "Close error\n");

        free(curr);
    }

    return;

}