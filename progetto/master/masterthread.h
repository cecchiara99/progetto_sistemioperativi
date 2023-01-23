#if !defined(MASTER_THREAD_H)
#define MASTER_THREAD_H

#define _POSIX_C_SOURCE 2001112L
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <queue/queue.h>
#include <sys/select.h>
#include <signal.h>
#include <threadpool/threadpool.h>
#include <util.h>
#include <message.h>
#include <conn.h>
#include <boundedqueue/boundedqueue.h>


#define THREAD_DEFAULT 4
#define QLEN_DEFAULT 8
#define DELAY_DEFAULT 0
#define MAX_LEN_STR 128
#define MAXFILENAME 256
#define MAXARG 500

typedef struct args{
    long n;
    long q;
    long delay;
    char* directory_name;
    char* file[MAXARG];
    long nelems;
}args_t;

typedef struct
{
    sigset_t *set; /// set dei segnali da gestire (mascherati)
    int sig_pipe;
} sigHandler_t;

typedef struct{
    
    long nelem;
    long size_q;
    long delay;
    long n_th;
    Queue_t *coda;
    
} argMaster_t;

typedef struct{
    BQueue_t* shared;
    long fine;
    long d;
    long n_e;
    long n_proc;
}argTask_t;


args_t* checkargs(int argc, char* args[]);

void usage(const char* argv0);

int isdot(const char dir[]);

void lsR(char* files[], const char *nd,long*n, int* index);

void *sigHandler(void *args);

void *master(void *args);

void *task(void *args);

#endif