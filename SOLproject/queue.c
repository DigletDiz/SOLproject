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
        perror("enqueue failed: q is NULL\n");
        return q;
    }
    node* new = (node*) malloc(sizeof(node));
    if (new == NULL) {
        perror("enqueue failed: malloc failed\n");
        return q;
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
        return NULL;
    }
    if(q->head == NULL) {
        perror("q is empty");
        return NULL;
    }

    node* popped = q->head;
    q->head = q->head->next;

    if(q->head == NULL) {
        q->tail = NULL;
    }

    return popped;
}