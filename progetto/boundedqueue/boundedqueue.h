#if !defined(BOUNDED_QUEUE_H)
#define BOUNDED_QUEUE_H

#include <pthread.h>

/** Struttura dati coda. FIFO
 *
 */
typedef struct BQueue {
    void   **buf;
    size_t   head;
    size_t   tail;
    size_t   qsize;
    size_t   qlen;
    pthread_mutex_t  m;
    pthread_cond_t   cfull;
    pthread_cond_t   cempty;
} BQueue_t;


BQueue_t *initBQueue(size_t n);

void deleteBQueue(BQueue_t *q, void (*F)(void*));

int    Bpush(BQueue_t *q, void *data);

void *Bpop(BQueue_t *q);


#endif /* BOUNDED_QUEUE_H */
