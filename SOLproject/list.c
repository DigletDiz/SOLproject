#include <list.h>


void listInsertHead(lnode** l, char* pathname) {
    lnode* new = (lnode*) malloc(sizeof(lnode));
    if(new == NULL) {
        perror("malloc failed");
        return;
    }

    new->pathname = pathname;
    new->next = *l;

    *l = new;

    return;
}


void listInsertTail(lnode** l, char* pathname) {
    lnode* new = (lnode*) malloc(sizeof(lnode));
    if(new == NULL) {
        perror("malloc failed");
        return;
    }

    new->pathname = pathname;
    new->next = NULL;

    if(*l == NULL) {
        *l = new;
    }
    else {
        lnode* curr = *l;
        while(curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new;
    }

    return;
}


int listFind(lnode* l, char* pathname) {
    while(l != NULL) {
        if(strcmp(l->pathname, pathname) == 0) {
            return 0; //file found
        }
        else {
            l = l->next;
        }
    }

    //file not found
    return -1;
}


void listRemove(lnode** l, char* pathname) {
    if(*l == NULL) {
        printf("List is empty\n");
        return;
    }
    lnode* prec = NULL;
    lnode* curr = *l;
    while(curr != NULL) {
        if(strcmp((*l)->pathname, pathname) == 0) {
            //file found
            if(prec == NULL) {
                free(curr);
                *l = NULL;
                return;
            }
            else {
                prec->next = curr->next;
                free(curr);
                return;
            }
        }
        else {
            prec = curr;
            curr = curr->next;
        }
    }

    //file not found
    printf("element not found\n");
    return;
}


void listDelete(lnode** l) {

    lnode* tmp;
    
    while(*l != NULL) {
        tmp = *l;
        *l = (*l)->next;
        free(tmp);
    }
    return;
}


void listDestroy(flist* fli) {

    if(fli == NULL) {
        perror("error: the list in null");
        return;
    }

    lnode* l = fli->head;
    listDelete(&l);
    free(fli);
    fli = NULL;

    return;
}


//just for icl_destroy
void listDestroyicl(void* fli) {

    listDestroy((flist*)fli);
    return;
}