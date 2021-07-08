#define UNIX_PATH_MAX 108
#include <api.h>


static int fdsocket;

int openConnection(const char* sockname, int msec, const struct timespec abstime) {

    //socket address set up
    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    //socket creation
    if((fdsocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return -1;
    }

    //waiting time
    struct timespec waitTime; 
    waitTime.tv_nsec = msec * 1000000;

    //current time
    int err = 0;
    int errnocpy = 0;
    struct timespec currTime;
    if((err = clock_gettime(CLOCK_REALTIME, &currTime)) == -1) {
        errnocpy = errno;
        close(fdsocket);
        errno = errnocpy;
        return -1;
    }

    while((err = connect(fdsocket, (struct sockaddr*)&sa, sizeof(sa))) == -1
            && currTime.tv_sec < abstime.tv_sec) {

        if((err = nanosleep(&waitTime, NULL)) == -1) {
            errnocpy = errno;
            close(fdsocket);
            errno = errnocpy;
            return -1;
        }
        if((err = clock_gettime(CLOCK_REALTIME, &currTime)) == -1) {
            errnocpy = errno;
            close(fdsocket);
            errno = errnocpy;
            return -1;        
        }
        
    }

    //è riuscito a connettersi
    if(err != -1) {
        return 0;
    }

    //è scaduto il tempo
    close(fdsocket);
    errno = ETIMEDOUT;
    return -1;
}


int closeConnection(const char* sockname) {

    if(sockname == NULL) {
        errno = EINVAL;
        return -1;
    }

    if(close(fdsocket) == -1) {
        return -1;
    }

    return 0;
}


int openFile(const char* pathname, int flags) {

    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }

    int err = 0;

    request* req = (request*) malloc(sizeof(request));
    if (req == NULL) {
        //perror("malloc failed\n");
        errno = ENOMEM;
        return -1;
    }
    memset(req, 0, sizeof(request));

    req->code = OPENFILE;
    strcpy(req->pathname, pathname);
    //printf("%s\n", req->pathname);
    req->flags = flags;

    //sending request
    if((err = writen(fdsocket, req, sizeof(request))) == -1) {
	    return -1;
    }

    free(req);

    int feedback;

    //Reading server's feedback
    if((err = readn(fdsocket, &feedback, sizeof(int))) == -1) {
	    return -1;
    }

    if(feedback == 0) {
        return 0;
    }
    else {
        errno = feedback;
        return -1;
    }

    return 0;
}


int readFile(const char* pathname, void** buf, size_t* size) {

    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }

    int err = 0;

    request* req = (request*) malloc(sizeof(request));
    if(req == NULL) {
        //perror("malloc failed\n");
        //errno = ENOMEM;
        return -1;
    }
    memset(req, 0, sizeof(request));

    req->code = READFILE;
    strcpy(req->pathname, pathname);
    //req->pathname = pathname;
    //printf("Codice: %d\n", req->code);
    //printf("Path: %s\n", req->pathname);
    //sending request
    if((err = writen(fdsocket, req, sizeof(request))) == -1) {
        free(req);
	    return -1;
    }

    free(req);

    int feedback;

    //Reading server's feedback
    if((err = readn(fdsocket, &feedback, sizeof(int))) == -1) {
	    return -1;
    }
    if(feedback == 0) {        
        reply* rep = (reply*) malloc(sizeof(reply));
        if(!rep) {
            //errno = ENOMEM;
            return -1;
        }

        if((err = readn(fdsocket, rep, sizeof(reply))) == -1) {
            free(rep);
	        return -1;
        }

        *size = strlen(rep->content); //////////////////////////////////////////////////////////////////////
        strcpy(*buf, rep->content);

        free(rep);
    }
    else {
        errno = feedback;
        return -1;
    }

    return 0;
}


int readNFiles(int N, const char* dirname) {

    if(dirname == NULL) {
        errno = EINVAL;
        return -1;
    }

    int err = 0;

    request* req = (request*) malloc(sizeof(request));
    if(req == NULL) {
        //perror("malloc failed\n");
        errno = ENOMEM;
        return -1;
    }
    memset(req, 0, sizeof(request));

    req->code = READNFILES;
    req->nfiles = N;

    //sending request
    if((err = writen(fdsocket, req, sizeof(request))) == -1) {
	    return -1;
    }

    free(req);

    int howmany;

    //Reading how many files I am receiving
    if((err = readn(fdsocket, &howmany, sizeof(int))) == -1) {
	    return -1;
    }

    int feedback;

    //Reading server's feedback
    if((err = readn(fdsocket, &feedback, sizeof(int))) == -1) {
	    return -1;
    }
    if(feedback != 0) {
        errno = feedback;
        return -1;
    }

    reply* rep = (reply*) malloc(sizeof(reply));
    if(!rep) {
        //errno = ENOMEM;
        return -1;
    }
    memset(rep, 0, sizeof(reply));

    //creating the realpath, creating directory if it doesn't exist
    char* realp = realpath(dirname, NULL);
    if(!realp) {
        mkdir(dirname, 0777);
        realp = realpath(dirname, NULL);
    }

    char* dir = (char*) malloc(sizeof(char)*BUFSIZE);
    strcpy(dir, realp);
  
    int dirsize;
    char* basenam;
    int i;
    for(i=0;i<howmany;i++) {
        if((err = readn(fdsocket, rep, sizeof(reply)) == -1)) {
	        return -1;
        }

        //preparing abspath of newfile
        basenam = basename(rep->pathname);
        strcpy(dir, realp);
        strcat(dir, "/");
        strcat(dir, basenam);

        FILE* newfile = fopen(dir, "w");
        fprintf(newfile, "%s", rep->content);
        fclose(newfile);
        //dir reset
        dirsize = strlen(dir);
        memset(dir, 0, dirsize);
        memset(rep, 0, sizeof(reply));
    }

    int filesread;
    if((err = readn(fdsocket, &filesread, sizeof(int)) == -1)) {
        //perror("Error in reading how many files have been read");
	    return -1;
    }

    free(realp);
    free(dir);
    free(rep);

    return filesread;
}


int writeFile(const char* pathname, const char* dirname);


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {

    if(pathname == NULL) {
        errno = EFAULT;
        return -1;
    }

    int err = 0;

    request* req = (request*) malloc(sizeof(request));
    if(req == NULL) {
        //perror("malloc failed\n");
        //errno = ENOMEM;
        return -1;
    }
    memset(req, 0, sizeof(request));

    req->code = APPENDTOFILE;
    strcpy(req->pathname, pathname);
    strcpy(req->toappend, (char*)buf);
    //req->pathname = pathname;
    //printf("Codice: %d\n", req->code);
    //printf("Path: %s\n", req->pathname);
    //sending request
    if((err = writen(fdsocket, req, sizeof(request))) == -1) {
	    return -1;
    }

    free(req);

    int feedback;

    //Reading server's feedback
    if((err = readn(fdsocket, &feedback, sizeof(int))) == -1) {
	    return -1;
    }
    if(feedback != 0) {
        errno = feedback;
        return -1;
    }

    return 0;
}


int lockFile(const char* pathname);


int unlockFile(const char* pathname);


int closeFile(const char* pathname) {

    if(pathname == NULL) {
        errno = ENOENT;
        return -1;
    }
    request* req = (request*) malloc(sizeof(request));
    if (req == NULL) {
        errno = ENOMEM;
        return -1;
    }
    memset(req, 0, sizeof(request));

    req->code = CLOSEFILE;
    strcpy(req->pathname, pathname);
    //printf("%s\n", req->pathname);

    //sending request
    int err = 0;
    if((err = writen(fdsocket, req, sizeof(request))) == -1) {
        return -1;
    }
    
    free(req);

    //Reading server's feedback
    int feedback;
    if((err = readn(fdsocket, &feedback, sizeof(int))) == -1) {
        return -1;
    }
    
    if(feedback == 0) {
        errno = 0;
        return 0;
    }
    else {
        errno = feedback;
        return -1;
    }
    return 0;
}


int removeFile(const char* pathname);