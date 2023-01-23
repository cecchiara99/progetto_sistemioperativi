#define _POSIX_C_SOURCE 2001112L
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <master/masterthread.h>
#include <collector/collector.h>

int main (int argc, char* argv[]){
    // controllo numero argomenti

    if (argc == 1)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // inizializzazione pipe per segnali
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd))
    {
        perror("creazione pipe");
        return EXIT_FAILURE;
    }

    //generazione processo figlio -> collector

    switch (pid = fork()){
        case -1:{
            perror("fork");
            return EXIT_FAILURE;
        }
        break;

        case 0:{ // sono il figlio
            // chiusura pipe in scrittura
            if (close(pipefd[1]) == -1)
            {
                perror("chiusura pipe scrittura - collector");
                return EXIT_FAILURE;
            }

            // blocco segnali

            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGQUIT);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGUSR1);

            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
            {
                fprintf(stderr, "FATAL ERROR\n");
                return EXIT_FAILURE;
            }

            // ignoro SIGPIPE
            struct sigaction s;
            memset(&s, 0, sizeof(s));
            s.sa_handler = SIG_IGN;

            if ((sigaction(SIGPIPE, &s, NULL)) == -1)
            {
                perror("sigaction");
                return EXIT_FAILURE;
            
            }

            //buffer per la pipe - ricezione segnali
            char buffer[256];

            //  instaurazione della connessione, sono il server

            int listenfd;

            // creazione del socket
            if ((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
            {
                perror("socket");
                return EXIT_FAILURE;
            }

            struct sockaddr_un serv_addr;
            memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sun_family = AF_UNIX;
            strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME) + 1);

            // collegamento al socket
            if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
            {
                perror("bind");
                unlink(SOCKNAME);
                return EXIT_FAILURE;
            }

            if (listen(listenfd, MAXBACKLOG) == -1)
            {
                unlink(SOCKNAME);
                perror("listen");
                return EXIT_FAILURE;
            }

            fd_set set, tmpset;
            FD_ZERO(&set);
            FD_ZERO(&tmpset);

            FD_SET(listenfd, &set);  // aggiungo il listener fd al master set
            FD_SET(pipefd[0], &set); // aggiungo il descrittore di lettura della signal_pipe

            // coda su cui fare lo storage dei file
            Queue_t *fileReceived = NULL;
            fileReceived = initQueue();

            volatile long termina = 0;

            while (termina == 0)
            {
                tmpset = set;

                if (select(FD_SETSIZE, &tmpset, NULL, NULL, NULL) == -1){ 
                    perror("select"); 
                    break;
                }

                int i;
            
                for (i = 0; i < FD_SETSIZE; ++i)
                {
                    if (FD_ISSET(i, &tmpset))
                    {
                        long connfd;
                        if (i == listenfd){ // e' una nuova richiesta di connessione
                            SYSCALL_EXIT("accept", connfd, accept(listenfd, (struct sockaddr *)NULL, NULL), "accept", "");
                            
                            if(connfd < 0){
                                perror("accept");
                                return EXIT_FAILURE;
                            }

                            FD_SET(connfd,&set);
                        
                        }else{
                            if (i == pipefd[0]){ // ricevuto un segnale

                                int nread;
                                SYSCALL_EXIT(read,nread,read(pipefd[0],buffer,256),"read","");
                                
                                if(nread < 0){
                                    perror("read");
                                    termina = 1;
                                    break;
                                }

                                buffer[nread] = '\0';

                                if (strncmp(buffer, "end", 3) == 0)//segnale di terminazione (uno dei)
                                {
                                    termina = 1;
                                    break;
                                }
                                if(strncmp(buffer,"stampa",6)==0){//segnale di stampa SIGUSR1
                                    
                                    stampacoda(fileReceived);
                                }
                            }else{//richiesta da parte di un client già connesso
                            
                                if(cmd(i,fileReceived)<0){
                                    int r;
                                    SYSCALL_EXIT(close, r, close(i), "close client con connfd = %ld",i);
                                    FD_CLR(i,&set);//rimozione del file descriptor dall'insieme di fd monitorati dalla select
                                }
                            }
                        }
                    }
                }
            }

            //ricevuto segnale di terminazione, stampo il risultato 
            stampacoda(fileReceived);

            // pulizia coda

            Node_t *tmp = fileReceived->head->next;
            while (tmp != NULL)
            {
                msg_t *m = tmp->data;

                if (m->str)
                    free(m->str);
                if (m)
                    free(m);
                tmp = tmp->next;
            }

            if (fileReceived)
                deleteQueue(fileReceived);

            if (close(listenfd) == -1)
            {
                perror("close socket");
                return EXIT_FAILURE;
            }

            unlink(SOCKNAME);

            return EXIT_SUCCESS;
            
        }break;

        default:
        { // sono il padre

            //chiusura pipe in lettura
            
            if(close(pipefd[0])==-1){
                perror("close pipe in lettura");
                return EXIT_FAILURE;
            }

            // blocco dei segnali
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGQUIT);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGUSR1);

            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
            {
                fprintf(stderr, "FATAL ERROR\n");
                return EXIT_FAILURE;
            }

            // spawn del thread che si occupa di gestire i segnali -> termina solo se riceve un segnale di terminazione
            pthread_t sighandler_thread;
            sigHandler_t handlerArgs = {&mask, pipefd[1]};

            if (pthread_create(&sighandler_thread, NULL, sigHandler, &handlerArgs) != 0)
            {
                fprintf(stderr, "errore nella creazione del signal handler thread\n");
                return EXIT_FAILURE;
            }

            // parsing degli argomenti passati da linea di comando 

            args_t *argMain = NULL;

            argMain = checkargs(argc, argv);

            if (!argMain)
            {
                fprintf(stderr, "parsing opzioni non riuscita\n");
                return EXIT_FAILURE;
            }

            //coda da cui il thread master preleverà i file da elaborare per passarli alla coda condivisa dai worker del threadpool

            Queue_t *files = NULL;
            files = initQueue();

            for (int i = 0; i < argMain->nelems; i++)
            {
                push(files, (char *)argMain->file[i]);
            }
            
            //spawn master

            argMaster_t args = {argMain->nelems, argMain->q, argMain->delay, argMain->n,files};

            pthread_t master_th;

            if (pthread_create(&master_th, NULL, master, &args) != 0)
            {
                fprintf(stderr, "errore nella creazione del signal handler thread\n");
                return EXIT_FAILURE;
            }

            //join master        

            pthread_join(master_th,NULL);

            //innesco terminazione signal_handler -> terminazione anche del processo collector

            pthread_kill(sighandler_thread, SIGINT);

            //attesa sig_handler
            
            pthread_join(sighandler_thread,NULL);

            // attesa della terminazione del processo figlio (collector)
            int status;
            pid = wait(&status);

            if(pid == -1){
                perror("wait");
                return EXIT_FAILURE;
            }

            //pulizia

            if(argMain->directory_name) free(argMain->directory_name);

            int i;

            for (i = argMain->nelems - 1; i >= 0; i--)
            {
                if (argMain->file[i])
                    free(argMain->file[i]);
            }

            if (argMain)
                free(argMain);

            if(files) deleteQueue(files);

            unlink (SOCKNAME);

            return EXIT_SUCCESS;

        }break;
    }

    //chiusura pipe entrambi i lati
    int r;
    SYSCALL_EXIT(close,r,close(pipefd[0]),"close pipefd[0]","");
    SYSCALL_EXIT(close, r, close(pipefd[1]), "close pipefd[1]", "");

    return EXIT_SUCCESS;
}