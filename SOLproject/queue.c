#include <stdio.h>
#include <stdlib.h>
#include <queue.h>

queue* qcreate() {
    queue* new = (queue*) malloc(sizeof(queue));
    if(new == NULL) {
        perror("malloc failed");
        return NULL;
    }

    new->head = NULL;
    new->tail = NULL;

    return new;
}

void enqueue(queue* q, const char op, void* dat) {
    if(q == NULL) {
        perror("enqueue failed: q is NULL\n");
        return;
    }
    node* new = (node*) malloc(sizeof(node));
    if (new == NULL) {
        perror("enqueue failed: malloc failed\n");
        return;
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

    return;
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