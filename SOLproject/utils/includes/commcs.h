#define BUFSIZE 256

typedef struct request{
    int code;
    char pathname[BUFSIZE];
    int flags;
    int nfiles;
} request;

typedef struct reply{
    char pathname[BUFSIZE];
    char content[BUFSIZE];
} reply;