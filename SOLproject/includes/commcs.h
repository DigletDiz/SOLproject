#if !defined(FILESIZE)
#define FILESIZE 1000000
#endif

#if !defined(PATHSIZE)
#define PATHSIZE 256
#endif

typedef struct request{
    int code;
    char pathname[PATHSIZE];
    char content[FILESIZE];
    int sizecontent;
    int flags;
    int nfiles;
    char toappend[FILESIZE];
    int sizeappend;
} request;

typedef struct reply{
    char pathname[PATHSIZE];
    char content[FILESIZE];
    size_t contentsize;
} reply;