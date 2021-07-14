#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500
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
int tokenappend(char* totokenize);

void print_usage(const char *programname) {
    printf("usage: %s -f filename -w dirname[,n=0] -W file1[,file2] -D dirname -r file1[,file2] -R [n=0] -d dirname  -t time -l file1[,file2] -u file1[,file2] -c file1[,file2] -p -h -a file1@\"toappend\" -v file1[,file2]\n", programname);
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
    
    while((opt = getopt(argc, argv, "f:w:W:D:r:R::d:t::l:u:c:pha:v:")) != -1) {
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
                tokenenqueue(optarg, 'r');
                read_flag = 1;
                break;
            case 'R':
                if(optarg != NULL) {
                    err = isNumber(optarg, &R_nfiles);
                    if(err == 1) {printf("Not a number: will be treated as 0");}
                    if(err == 2) {printf("Overflow/underflow: will be treated as 0");}
                }
                enqueue(q, 'R', (void*)&R_nfiles, NULL);
                read_flag = 1;
                break;
            case 'd':
                d_dir = optarg;
                break;
            case 't':
                if(optarg != NULL) {
                    err = isNumber(optarg, &t_time);
                    if(err == 1) {printf("Not a number: will be treated as 0");}
                    if(err == 2) {printf("Overflow/underflow: will be treated as 0");}
                    if(t_time > 999) {t_time = 999;}
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
            {
                int checka = tokenappend(optarg);  
                if(checka == -1) {
                    fprintf(stderr, "error append: invalid format (filename@\"stringtoappend\")\n");
                }  
                break;
            }
            case 'v':
            {
                tokenenqueue(optarg, 'v');
                break;
            }
            default:
                if(optopt == 'd' || optopt == 'r')
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
    char* toapnd;
    char* basenam;
    char* realp;
    size_t sis = 0;
    node* curr;

    while(q->head != NULL) {
        curr = pop(q);
        currdata = (char*)curr->data;

        switch(curr->opt) {
            case 'r':
            {
                SYSCALL_PRINT("openFile", err, openFile(currdata, NOFLAGS), "cannot open", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("openFile %s: success\n", currdata);}
                buf = (char*) malloc(sizeof(char)*FILESIZE);
                if(!buf) {
                    perror("buffer");
                    break;
                }
                SYSCALL_PRINT("readFile", err, readFile(currdata, (void**)&buf, &sis), "cannot read", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("readFile %s: success with %ld bytes read\n", currdata, sis);}
                printf("Contenuto letto: %s\n", buf);
                if(d_dir != NULL) {
                    //creating dir if it doesn't exist
                    realp = realpath(d_dir, NULL);
                    if(!realp) {
                        mkdir(d_dir, 0777);
                        realp = realpath(d_dir, NULL);
                    }
                    //preparing abspath of newfile
                    basenam = basename(currdata);
                    strcat(realp, "/");
                    strcat(realp, basenam);

                    FILE* newfile = fopen(realp, "w");
                    fprintf(newfile, "%s", buf);
                    fclose(newfile);
                }
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
                    printf("readNfiles: success. Files read: %d\n", nrfiles);
                }
                break;
            }
            case 'a':
            {
                toapnd = curr->toapp;
                SYSCALL_PRINT("openFile", err, openFile(currdata, NOFLAGS), "cannot open", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("openFile %s: success\n", currdata);}
                sis = strlen(toapnd);
                SYSCALL_PRINT("appendToFile", err, appendToFile(currdata, toapnd, sis, NULL), "cannot append", p_print, currdata);
                /////controlla che sis abbia il numero giusto
                if(err == 0 && p_print == 1) {printf("appendToFile %s of size %ld: success\n", currdata, sis);} 
                /////
                SYSCALL_PRINT("closeFile", err, closeFile(currdata), "cannot close", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("closeFile %s: success\n", currdata);}
                break;
            }
            case 'v':
            {
                SYSCALL_PRINT("myWriteFile", err, myWriteFile(currdata), "cannot write", p_print, currdata);
                if(err == 0 && p_print == 1) {printf("myWriteFile %s: success\n", currdata);}
            }
            default: /* '?' */
                break;
                
        }
       
        free(curr);

        usleep(t_time * 1000);
    }

    return;

}


void tokenenqueue(char* totokenize, char opt) {
    char* token;
    char* rest = totokenize;
  
    while((token = strtok_r(rest, ",", &rest))) {
        enqueue(q, opt, (void*)token, NULL);
    }
  
    return;
}


int tokenappend(char* totokenize) {
    char* rest = totokenize;
  
    char* pathname = strtok_r(rest, "@", &rest);
    char* toappend = strtok_r(rest, "\0", &rest);
  
    if(!(pathname) || !(toappend)) { 
        return -1;
    }

    enqueue(q, 'a', (void*)pathname, toappend);

    return 0;
}