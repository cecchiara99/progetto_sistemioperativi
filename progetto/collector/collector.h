#if !defined(COLLECTOR_H)
#define COLLECTOR_H

#define _POSIX_C_SOURCE 2001112L
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>
#include <conn.h>
#include <util.h>
#include <message.h>
#include <queue/queue.h>

#define MAXFILENAME 256

void stampacoda(Queue_t *q);

Queue_t *pushOrdered(Queue_t *coda, msg_t *msg);

int cmd(int connfd, Queue_t *coda);

#endif

