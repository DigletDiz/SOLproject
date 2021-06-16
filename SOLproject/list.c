typedef struct node {
    char opt;
    void* data;
    struct node* next;
} node;


node* listInsertHead(node* l, const char op, void* dat) {
    node* new = (node*) malloc(sizeof(node));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->opt = op;
    new->data = dat;
    new->next = l;

    l = new;

    return l;
}

node* listInsertTail(node* l, const char op, void* dat) {
    node* new = (node*) malloc(sizeof(node));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->opt = op;
    new->data = dat;
    new->next = NULL;

    if(l == NULL) {
        l = new;
    }   
    else {
        node* tmp = l;
        while(tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = new;
    }

    return l;
}

void listDelete(node** l) {

    node* tmp;
    
    while(*l != NULL) {
        tmp = *l;
        *l = (*l)->next;
        free(tmp);
    }
}