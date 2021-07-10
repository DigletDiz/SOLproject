#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <time.h>
#include <string.h>

#include <conn.h>
#include <util.h>
#include <queue.h>
#include <list.h>
#include <api.h>


#define SOCKNAME "./cs_sock"
#define UNIX_PATH_MAX 108

void optParse(int argc, char* argv[]);
void optExe();
void tokenenqueue(char* totokenize, char opt);
int tokenappend(char* totokenize, char** pathname, char** toappend);

void print_usage(const char *programname) {
    printf("usage: %s -f filename -w dirname[,n=0] -W file1[,file2] -D dirname -r file1[,file2] -R [n=0] -d dirname  -t time -l file1[,file2] -u file1[,file2] -c file1[,file2] -p -h\n", programname);
}

static queue* q;
static char* f_sockname = NULL;
static char* d_dir = NULL;
static int t_time = 0; 
static int p_print = 0;


int main(int argc, char* argv[]) {

    q = qcreate();

    optParse(argc, argv);
    //printf("Fuori dal parse\n");

    char* sockname = (f_sockname) ? f_sockname : SOCKNAME;

    int err;
    struct timespec abstime;
    abstime.tv_sec = 5;
    if((err = openConnection(sockname, 100, abstime)) == -1) {
        perror("Connection failed\n");
        return -1;
    }
    printf("Connected!\n");

    optExe();
    //printf("Fuori dall'exe\n");
    qdestroy(q);

    return 0; 
}

void optParse(int argc, char* argv[]) {
    int err;
    int opt;
    int R_nfiles = 0;
    int read_flag = 0;
    
    while((opt = getopt(argc, argv, "f:w:W:D:r:R::d:t::l:u:c:pha:")) != -1) {
        switch(opt) {
            case 'f':
                if(f_sockname != NULL) {
                    fprintf(stderr, "Option -%d already setted\n", opt);  
                }
                else {
                    f_sockname = optarg;
                }
                break;
            case 'w':
                printf("Option -%d not supported\n", opt);
                break;
            case 'W':
                printf("Option -%d not supported\n", opt);
                break;
            case 'D':
                printf("Option -%d not supported\n", opt);
                break;
            case 'r':
                //printf("Sono in parse opt = r\n");
                tokenenqueue(optarg, 'r');
                read_flag = 1;
                break;
            case 'R':
                if(optarg != NULL) {
                    err = isNumber(optarg, (long*)&R_nfiles);
                    if(err == 1) {printf("Not a number: will be treated as 0");}
                    if(err == 2) {printf("Overflow/underflow: will be treated as 0");}
                }
                enqueue(q, 'R', (void*)&R_nfiles);
                read_flag = 1;
                break;
            case 'd':
                d_dir = optarg;
                break;
            case 't':
                if(optarg != NULL) {
                    err = isNumber(optarg, (long*)&t_time);
                    if(err == 1) {printf("Not a number: will be treated as 0");}
                    if(err == 2) {printf("Overflow/underflow: will be treated as 0");}
                }
                break;
            case 'l':
                printf("Option %d not supported\n", opt);
                break;
            case 'u':
                printf("Option %d not supported\n", opt);
                break;
            case 'c':
                printf("Option %d not supported\n", opt);
                break;
            case 'p':
                if(p_print == 1) {
                    fprintf(stderr, "Option -%d already setted\n", opt);  
                }
                else {
                    p_print = 1;
                }
                break;
            case 'h':
                qdestroy(q);
                print_usage(argv[0]);
                _exit(EXIT_SUCCESS);
            case 'a':
                enqueue(q, 'a', (void*)optarg);            
                break;
            default: /* '?' */
                if (optopt == 'd' || optopt == 'r')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                break;
        }
    }
    if(d_dir != NULL && read_flag == 0) {
        fprintf(stderr, "Option -d must be paired with option -r or -R\n");
    }

    int index;
    for(index = optind; index < argc; index++) {
        fprintf(stderr, "Non-option argument %s\n", argv[index]);
    }
}


void optExe() {

    int err = 0;
    char* buf;
    char* currdata;
    //char* append = " sono Daniele";
    size_t sis = 0;
    node* curr;
    char* pathname;
    char* toappend;

    while(q->head != NULL) {
        curr = pop(q);
        currdata = (char*)curr->data;

        switch(curr->opt) {
            case 'r':
            {
                SYSCALL_PRINT("openFile", err, openFile(currdata, NOFLAGS), "cannot open", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("openFile %s: success\n", currdata);}
                buf = (char*) malloc(sizeof(char)*BUFSIZE);
                SYSCALL_PRINT("readFile", err, readFile(currdata, (void**)&buf, &sis), "cannot read", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("readFile %s: success with %ld bytes read\n", currdata, sis);}
                printf("Contenuto letto: %s\n", buf);
                free(buf);
                SYSCALL_PRINT("closeFile", err, closeFile(currdata), "cannot close", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("closeFile %s: success\n", currdata);}
                break;
            }
            case 'R':
            {
                int nrfiles;
                nrfiles = readNFiles(*((int*)(curr->data)), d_dir);
                if(nrfiles == -1 && p_print == 1) {
                    perror("readNfiles");
                }
                else if(nrfiles >= 0 && p_print == 1) {
                    printf("readNfiles: success. Files read: %d", nrfiles);
                }
                break;
            }
            case 'a':
            {
                err = tokenappend(currdata, &pathname, &toappend);
                if(err == -1) {fprintf(stderr, "error append: invalid format (filename@\"stringtoappend\")\n");break;}

                SYSCALL_PRINT("openFile", err, openFile(pathname, NOFLAGS), "cannot open", p_print, pathname);
                if(err == 0 && p_print == 1) {printf("openFile %s: success\n", pathname);}
                //char* b = " SEEEEEEEEEEEEEEEEEES";
                int s = strlen(toappend);
                SYSCALL_PRINT("appendToFile", err, appendToFile(pathname, toappend, s, NULL), "cannot append", p_print, pathname);
                if(err == 0 && p_print == 1) {printf("appendToFile %s: success\n", pathname);}
                SYSCALL_PRINT("closeFile", err, closeFile(pathname), "cannot close", p_print, pathname);
                if(err == 0 && p_print == 1) {printf("closeFile %s: success\n", pathname);}
                break;
            }
            default: /* '?' */
                break;
                
        }
        //printf("Sono in exe\n");
        //curr = pop(q);
        //curr->opt

        //err = readNFiles(3, "ciccio");
        //if(err != 0) {printf("vabbe t'apposto %d\n", err);}
        /*SYSCALL_PRINT("openFile", err, openFile((char*)(curr->data), 0), "Open error\n", "");
        if(err == 0) {printf("File aperto con successo\n");}
        SYSCALL_PRINT("readFile", err, readFile((char*)(curr->data), (void**)&buf, &sis), "Read error\n", "");
        if(err == 0) {printf("File letto con successo\n");printf("Contenuto letto: %s\n", buf);}
        SYSCALL_PRINT("closeFile", err, closeFile((char*)(curr->data)), "Close error\n", "");
        if(err == 0) {printf("File chiuso con successo\n");}*/
        /*SYSCALL_PRINT("openFile", err, openFile((char*)(curr->data), 0), "Open error\n", "");
        if(err == 0) {printf("File aperto con successo\n");}
        SYSCALL_PRINT("appendToFile", err, appendToFile((char*)(curr->data), (void*)append, sis, NULL), "Append error\n", "");
        if(err == 0) {printf("Append: success\n");}
        SYSCALL_PRINT("closeFile", err, closeFile((char*)(curr->data)), "Close error\n", "");
        if(err == 0) {printf("File chiuso con successo\n");}*/

        //err = readNFiles(3, "ciccio");
       
        free(curr);
    }

    return;

}


void tokenenqueue(char* totokenize, char opt) {
    char* token;
    char* rest = totokenize;
  
    while((token = strtok_r(rest, ",", &rest))) {
        enqueue(q, opt, (void*)token);
    }
  
    return;
}


int tokenappend(char* totokenize, char** pathname, char** toappend) {
    char* rest = totokenize;
  
    *pathname = strtok_r(rest, "@", &rest);
    *toappend = strtok_r(rest, "\0", &rest);
  
    if(!(*pathname) || !(*toappend)) { 
        return -1;
    }

    return 0;
}