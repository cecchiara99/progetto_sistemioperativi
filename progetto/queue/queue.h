#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>

/** Elemento della coda.
 *
 */
typedef struct Node {
    void        * data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
    Node_t        *head;    // elemento di testa
    Node_t        *tail;    // elemento di coda 
    unsigned long  qlen;    // lunghezza 
    pthread_mutex_t qlock;
    pthread_cond_t  qcond;
} Queue_t;

Queue_t *initQueue();

void deleteQueue(Queue_t *q);

int    push(Queue_t *q, void *data);

void  *pop(Queue_t *q);

#endif /* QUEUE_H_ */
