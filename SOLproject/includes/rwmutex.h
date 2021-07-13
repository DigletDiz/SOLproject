#include <pthread.h>

typedef struct rwmutex {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int nreaders;
    int waiting_writers;
    int writing;
} rwmutex;