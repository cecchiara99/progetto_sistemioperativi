#include <master/masterthread.h>

//variabile globale per la gestione dei segnali
static volatile sig_atomic_t endSig = 0;

void usage(const char *argv0)
{
    fprintf(stderr, "use: %s -n <n_threads> -q <q_len> -d <name_directory> -t <delay> [file1...fileN]\n", argv0);
}

int isdot(const char dir[])
{
    int l = strlen(dir);

    if ((l > 0 && dir[l - 1] == '.'))
        return 1;
    return 0;
}

//procedura per navigare ricorsivamente una directory
void lsR(char* files[], const char *nd,long * n, int* index)
{
    //stat della directory passata come argomento
    struct stat statbuffer;
    int r;
    SYSCALL_EXIT(stat, r, stat(nd, &statbuffer), "Facendo la stat di: %s; errno: %d\n", nd, errno);

    DIR *dir;

    //controllo che sia una directory
    CHECK_EQ_EXIT(opendir,(dir=opendir(nd)),NULL,"opendir","");

    struct dirent *file;

    //ciclo per la lettura dei file all'interno della directory
    while ((errno = 0, file = readdir(dir)) != NULL){
        struct stat filebuf;
        int len1 = strlen(nd);
        int len2 = strlen(file->d_name);
        
        if (len1 + len2 + 2 > MAXFILENAME){
                fprintf(stderr, "Nome file troppo lungo\n");
                return;
        }

        //costruzione del pathname
        char filename[MAXFILENAME];

        strncpy(filename, nd, len1);

        filename[len1] = '\0';

        strncat(filename, "/", 2);

        strncat(filename, file->d_name, len2);

        filename[(len1 + len2 + 2)] = '\0';

        //controllo del tipo di file

        SYSCALL_EXIT(stat, r, stat(filename, &filebuf), "Facendo la stat di: %s; errno: %d\n", filename, errno);

        if (S_ISDIR(filebuf.st_mode)){
            if (!isdot(filename)){
                lsR(files, filename,n,index); //chiamata ricorsiva, il file è una directory
            }
        }
        else{// (caso base) è un file, controllo che sia regolare
            if (S_ISREG(filebuf.st_mode)){
                
                CHECK_EQ_EXIT(malloc, (files[*index] = (char *)malloc(sizeof(char) * len1 + len2 + 2)),NULL,"malloc","");

                //aggiunta del nome del file alla lista dei files da sottoporre successivamente ai worker del threadpool
                files[*index][len1+len2+1] = '\0';
                strncpy(files[*index], filename, len1 + len2 + 2);

                *n+=1;
                *index+=1;
            }
            else{
                perror("file non regolare");
                print_error("errore nel file: %s", filename);
                return;
            }
        }
            
    }
    if (errno != 0)
    perror("readdir");

    int p;

    SYSCALL_EXIT(closedir,p,closedir(dir),"closedir","");

    return;
}

//funzione che restituisce la struttura contenente le opzioni passate da linea di comando
args_t* checkargs(int argc, char* args[]){

    args_t* arg = NULL;

    //allocazione della struttura dati
    CHECK_EQ_EXIT(malloc,(arg = (args_t*)malloc(sizeof(args_t))),NULL,"malloc","");
    
    int opt;
    
    
    arg->n  = THREAD_DEFAULT;
    arg->q = QLEN_DEFAULT;
    arg->delay = DELAY_DEFAULT;
    arg->directory_name = NULL;
    arg->nelems = 0;

    int i = 0;
    int *j = &i;

    // parsing degli argomenti da linea di comando

    while ((opt = getopt(argc, args, "-:n:q:t:d:")) != -1)
    {
        switch (opt){
        
        case 'n':{
            if (isNumber(optarg, &arg->n) != 0)
            {
                fprintf(stderr, "l'argomento dell'opzione -n deve essere un numero\n");
                usage(args[0]);
                return NULL;
            }
        }
        break;

        case 'q':{
            if (isNumber(optarg, &arg->q) != 0)
            {
                fprintf(stderr, "l'argomento dell'opzione -q deve essere un numero\n");
                usage(args[0]);
                return NULL;
            }
        }
        break;

        case 't':
        {
            if (isNumber(optarg, &arg->delay) != 0)
            {
                fprintf(stderr, "l'argomento dell'opzione -t deve essere un numero\n");
                usage(args[0]);
                return NULL;
            }
        }
        break;

        case 'd':
        {
            if (optarg == NULL)
                return NULL;
            CHECK_EQ_EXIT(malloc, arg->directory_name = (char *)malloc(sizeof(char) * MAXFILENAME), NULL, "malloc directory %s", arg->directory_name);
            int len = strlen(optarg);
            strncpy(arg->directory_name, optarg, MAXFILENAME);

            if (arg->directory_name[len - 1] == '/')
                arg->directory_name[len - 1] = '\0';
            else
                arg->directory_name[len] = '\0';

            lsR(arg->file, arg->directory_name,&arg->nelems,j);
        }
        break;

        case ':':
        {
            printf("l'opzione '-%c' richiede un argomento\n", optopt);
            return NULL;
        }
        break;

        case '?':
        { // restituito se getopt trova una opzione non riconosciuta
            printf("l'opzione '-%c' non e' gestita\n", optopt);
            return NULL;
        }
        break;

        
        case 1:
        {
            int len = strlen(args[optind - 1]);
            
            // controllo se l'argomento passato è un file irregolare, altriemnti termino
            struct stat stabuf;
            int r;
            SYSCALL_EXIT(stat, r, stat(args[optind - 1], &stabuf), "facendo stat del nome: %s; errno = %d", args[optind - 1], errno);

            if (!S_ISDIR(stabuf.st_mode))
            {
                struct stat filebuf;
                if (len > MAXFILENAME)
                {
                    fprintf(stderr, "nome file troppo lungo!\n");
                    return NULL;
                }
                SYSCALL_EXIT(stat, r, stat(args[optind - 1], &filebuf), "facendo stat del nome: %s; errno = %d", args[optind - 1], errno);

                if (S_ISREG(stabuf.st_mode)) 
                {
                    CHECK_EQ_EXIT(malloc, (arg->file[*j] = (char *)malloc(sizeof(char) * MAXFILENAME)), NULL, "malloc", "");
                    strncpy(arg->file[*j], args[optind - 1], MAXFILENAME);
                    arg->file[*j][len+1] = '\0';
                    arg->nelems++;
                    *j+=1;
                }
                else
                {
                    perror("file non regolare");
                    print_error("errore nel file: %s", args[optind - 1]);
                    return NULL;
                }
            }
            else
            {
                if (args[optind - 1][len - 1] == '/')
                    args[optind - 1][len - 1] = '\0';
                lsR(arg->file, args[optind - 1], &arg->nelems, j);
            }
        }
        break;
        }
    }
    return arg;
}

//funzione associata al thread per la gestione dei segnali
void *sigHandler(void *args)
{
    sigset_t *set = ((sigHandler_t *)args)->set;
    int fd_pipe = ((sigHandler_t *)args)->sig_pipe;

    // ignoro SIGPIPE

    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;

    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("sigaction");
        return NULL;
    }

    while (endSig == 0)
    {

        int sig;

        // attesa passiva della ricezione del segnale
        int r = sigwait(set, &sig);
        
        char buffer[256];

        if (r != 0)
        {
            errno = r;
            perror("FATAL ERROR 'sigwait'");
        }

        switch (sig)
        {
        // segnali di terminazione
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
        case SIGQUIT:
        {
            
            strcpy(buffer, "end");
            int len = strlen(buffer);
            buffer[len] = '\0';
            int r;
            SYSCALL_EXIT(write,r,write(fd_pipe,buffer,len+1),"write su pipe","");
            endSig=1;
        }
        break;
        // segnale di stampa
        case SIGUSR1:
        {
            
            strcpy(buffer, "stampa");
            int len = strlen(buffer);
            buffer[len] = '\0';
            int r;
            SYSCALL_EXIT(write, r, write(fd_pipe, buffer, len + 1), "write su pipe", "");
        }
        break;

        default:;
        }

        
    }
    return NULL;
}

// funzione associata al thread che si occupa dell'invio dei risultati presso il processo collector
void *master(void *args)
{

    long nelem = ((argMaster_t *)args)->nelem;
    long q_size = ((argMaster_t *)args)->size_q;
    long delay = ((argMaster_t *)args)->delay;
    long thread = ((argMaster_t *)args)->n_th;
    Queue_t* coda = ((argMaster_t*)args)->coda;

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
        return NULL;
    }

    // ignoro SIGPIPE
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;

    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("sigaction");
        return NULL;
    }


    //coda condivisa dai worker del pool
    BQueue_t *shared = initBQueue(q_size);

    //threadpool
    threadpool_t *pool = createThreadPool(thread, thread);

    volatile long termina = 0;
    volatile long n_processati = 0;


    //sottomissione dei thread nel pool
    for (int i = 0; i < thread; i++)
    {
        int r;
        argTask_t taskArg = {shared, (long)&termina, delay, nelem, (volatile long)&n_processati};
        if((r=addToThreadPool(pool, (void *)task, &taskArg))<0){
            perror("pool");
            destroyThreadPool(pool, 0);
            if (shared)
                deleteBQueue(shared, NULL);
            return NULL;
        } 
    }


    //push dei file da elaborare sulla coda condivisa

    int j = 0;
    while (endSig == 0){

        if(j == nelem){
            break;
        }
        
        char* tmp = NULL;

        tmp = (char*)pop(coda);

        int len = strlen(tmp);

        tmp[len] = '\0';
        
        Bpush(shared, (char *)tmp);

        ++j;

    }
    

    if(endSig==1) termina = 1;
    
    destroyThreadPool(pool, 0);

    if (shared)
        deleteBQueue(shared, NULL);

    return NULL;
}

// funzione associata ai thread del pool
void *task(void *args){

    // passaggio degli argomenti
    BQueue_t *sh = ((argTask_t *)args)->shared;
    long* termina = (long*)((argTask_t *)args)->fine;
    long delay = ((argTask_t *)args)->d;
    long nelems = ((argTask_t*)args)->n_e;
    long* num_ele_proc = (long*)((argTask_t*)args)->n_proc;

    struct timespec ts;

    ts.tv_sec = delay / 1000;
    ts.tv_nsec = (delay % 1000) * 1000000;

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
        return NULL;
    }

    // ignoro SIGPIPE
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;

    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("sigaction");
        return NULL;
    }

    // ciclo connessione al server

    struct sockaddr_un serv_addr;
    int sockfd;
    SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME) + 1);

    int r;

    while ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) == -1)
    {
        SYSCALL_EXIT(sleep,r,sleep(1),"sleep","");
    }

    fd_set set, tmpset;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);

    // finche' ci sono elementi da elaborare e non ho riceuto un segnale di terminazione, continuo a ciclare
    do{

        *num_ele_proc = *num_ele_proc + 1;

        if (*num_ele_proc > nelems)
            break;

        tmpset = set;
        
        int rS;

        struct timeval timeout = {0, 10000}; // 100 milliseconds
        if ((rS = select(FD_SETSIZE, &tmpset, NULL, NULL, &timeout)) < 0)
        {
            perror("select");
            break;
        }

        if(rS >= 0){

            if (*num_ele_proc > nelems || *termina == 1)
                break;
        }
        
        // eventuale delay
        if (delay > 0)
        {
            SYSCALL_EXIT(nanosleep, r, nanosleep(&ts, &ts),"nanosleep","");
        }        

        if (*num_ele_proc >= nelems && sh->qlen==0)
            break;

       

        char *file = (char *)Bpop(sh);

        if(file == NULL) break;

        FILE *filen = NULL;

        // apro il file
        CHECK_EQ_EXIT(fopen, (filen = fopen(file, "r")), NULL, "fopen %s", file);

        // calcolo del risultato
        long res = 0;
        int c = 0;
        long tmp = 0;

        while (fread(&tmp, sizeof(long), 1, filen) != 0)
        {
            res += (c * tmp);
            ++c;
        }

        // result stringa da inviare
        char *result = NULL;
        CHECK_EQ_EXIT(malloc, (result = (char *)malloc(sizeof(char) * MAXFILENAME)), NULL, "malloc", "");

        sprintf(result, "%ld ", res);
        strcat(result, file);

        // fine calcolo, chiusura del file
        int k;
        SYSCALL_EXIT(fclose, k, (fclose(filen)), "fclose", "");

        // invio del file
        int n = strlen(result) + 1;
        result[n] = '\0';
        int r;
        
        if(endSig==0){
            SYSCALL_EXIT("writen", r, writen(sockfd, &n, sizeof(int)), "write", "");
            SYSCALL_EXIT("writen", r, writen(sockfd, result, n * sizeof(char)), "write", "");
            if(result) free(result);
        }
        
        
        else{
            if(result) free(result);
            break;
        }
        

        if (*num_ele_proc >= nelems)
            break;

    } while (*termina == 0 && *num_ele_proc <= nelems);

    
    SYSCALL_EXIT(close, r, close(sockfd), "close", "");


    return NULL;
}
