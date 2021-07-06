#define UNIX_PATH_MAX 108
#include <api.h>


static int fdsocket;

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
/*Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
scadere del tempo assoluto ‘abstime’ specificato come terzo argomento. Ritorna 0 in caso di successo, -1 in caso
di fallimento, errno viene settato opportunamente.*/

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
/*Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/

    if(sockname == NULL) {
        errno = EFAULT;
        return -1;
    }

    if(close(fdsocket) == -1) {
        return -1;
    }

    return 0;
}


int openFile(const char* pathname, int flags) {
/*Richiesta di apertura o di creazione di un file. La semantica della openFile dipende dai flags passati come secondo
argomento che possono essere O_CREATE ed O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già
memorizzato nel server, oppure il file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un
errore. In caso di successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture possono
avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il file viene
aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o scrivere il file ‘pathname’ è il
processo che lo ha aperto. Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile,
descritta di seguito.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
    if(pathname == NULL) {
        errno = EFAULT;
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
/*Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata sullo heap nel
parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In
caso di errore, ‘buf‘e ‘size’ non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene
settato opportunamente.*/
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

    req->code = READFILE;
    strcpy(req->pathname, pathname);
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
    if(feedback == 0) {        
        reply* rep = (reply*) malloc(sizeof(reply));
        if(!rep) {
            //errno = ENOMEM;
            return -1;
        }

        if((err = readn(fdsocket, rep, sizeof(reply))) == -1) {
	        return -1;
        }

        strcpy(*buf, rep->content);
    }
    else {
        errno = feedback;
        return -1;
    }

    return 0;
}


int readNFiles(int N, const char* dirname) {
/*Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client. Se il server
ha meno di ‘N’ file disponibili, li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di successo (cioè ritorna il n. di file
effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.*/
    if(dirname == NULL) {
        errno = EFAULT;
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
/*Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il
file eventualmente spedito dal server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere
scritto in ‘dirname’; Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);
/*Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int lockFile(const char* pathname);
/*In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int unlockFile(const char* pathname);
/*Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/


int closeFile(const char* pathname) {
/*Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
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
/*Rimuove il file cancellandolo dal file storage server. L’operazione fallisce se il file non è in stato locked, o è in
stato locked da parte di un processo client diverso da chi effettua la removeFile.*/