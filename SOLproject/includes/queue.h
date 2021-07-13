#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    char opt;
    void* data;
    char* toapp;
    struct node* next;
} node;

typedef struct queue {
    node* head;
    node* tail;
} queue;


queue* qcreate();
void enqueue(queue* q, const char op, void* dat, char* toapp);
node* pop(queue* q);
void qdestroy(queue* q);
void headenqueue(queue* q, const char op, void* dat, char* toappend);