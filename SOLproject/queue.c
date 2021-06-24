typedef struct node {
    char opt;
    void* data;
    struct node* next;
} node;

typedef struct queue {
    node* head;
    node* tail;
} queue;


queue* qcreate() {
    queue* new = (queue*) malloc(sizeof(queue));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->head = NULL;
    new->tail = NULL;

    return new;
}

queue* enqueue(queue* q, const char op, void* dat) {
    if(q == NULL) {
        perror("q is NULL");
        return EXIT_FAILURE;
    }
    node* new = (node*) malloc(sizeof(node));
    if (new == NULL) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    new->opt = op;
    new->data = dat;
    new->next = NULL;

    if(q->head == NULL) {
        q->head = new;
        q->tail = new;
    }   
    else {
        q->tail->next = new;
        q->tail = new;
    }

    return q;
}

node* pop(queue* q) {
    if(q == NULL) {
        perror("q is NULL");
        return EXIT_FAILURE;
    }
    if(q->head == NULL) {
        perror("q is empty");
        return EXIT_FAILURE;
    }

    node* popped = q->head;
    q->head = q->head->next;

    if(q->head == NULL) {
        q->tail = NULL;
    }

    return popped;
}

/*node* listInsertHead(node* l, const char op, void* dat) {
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
}*/
/*void listDelete(node** l) {

    node* tmp;
    
    while(*l != NULL) {
        tmp = *l;
        *l = (*l)->next;
        free(tmp);
    }
}*/