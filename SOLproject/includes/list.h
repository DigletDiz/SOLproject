#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFSIZE 256

typedef struct lnode {
    char pathname[BUFSIZE];
    struct lnode* next;
} lnode;

typedef struct flist {
    lnode* head;
} flist;


void listInsertHead(lnode** l, char* pathname);
void listInsertTail(lnode** l, char* pathname);
int listFind(lnode* l, char* pathname);
void listRemove(lnode** l, char* pathname);
void listDelete(lnode** l);
void listDestroy(flist* fli);
void listDestroyicl(void* fli);