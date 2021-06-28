typedef struct lnode {
    char* pathname;
    struct lnode* next;
} lnode;


lnode* listInsertHead(lnode* l, char* pathname) {
    lnode* new = (lnode*) malloc(sizeof(lnode));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->pathname = pathname;
    new->next = l;

    l = new;

    return l;
}


lnode* listInsertTail(lnode* l, char* pathname) {
    lnode* new = (lnode*) malloc(sizeof(lnode));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->pathname = pathname;
    new->next = NULL;

    if(l == NULL) {
        l = new;
    }
    else {
        lnode* curr = l;
        while(curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new;
    }

    return l;
}


lnode* listFind(lnode* l, char* pathname) {
    while(l != NULL) {
        if(l->pathname == pathname) {
            return l; //file found
        }
        else {
            l = l->next;
        }
    }

    //file not found
    return NULL;
}


lnode* listRemove(lnode* l, char* pathname) {
    if(l == NULL) {
        printf("List is empty\n");
        return NULL;
    }
    lnode* prec = NULL;
    lnode* curr = l;
    while(curr != NULL) {
        if(l->pathname == pathname) {
            //file found
            if(prec == NULL) {
                free(curr);
                return NULL;
            }
            else {
                prec->next = curr->next;
                free(curr);
                return l;
            }
        }
        else {
            prec = curr;
            curr = curr->next;
        }
    }

    //file not found
    printf("element not found\n");
    return l;
}

void listDelete(lnode** l) {

    lnode* tmp;
    
    while(*l != NULL) {
        tmp = *l;
        *l = (*l)->next;
        free(tmp);
    }
}