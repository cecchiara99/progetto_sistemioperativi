#include <collector/collector.h>

// procedura di stampa della coda contenente i risultati ricebuti
void stampacoda(Queue_t *coda)
{
    if (coda->head->next == NULL)
    {
        return;
    }
    Node_t *tmp = coda->head->next;
    while (tmp != NULL)
    {
        msg_t *m = tmp->data;
        fprintf(stdout, "%s\n", (char *)m->str);
        tmp = tmp->next;
    }
    return;
}

// funzione che restituisce la coda in cui sono memorizzati i file dopo l'inserimento ordinato di un nuovo risultato
Queue_t *pushOrdered(Queue_t *coda, msg_t *msg)
{

    if (coda->head->next == NULL)
    { // la coda è vuota

        push(coda, (msg_t *)msg);
        return coda;
    }

    msg_t *tmp_m = (coda->head->next)->data; // primo elemento della coda

    if ((msg->res) <= (tmp_m->res))
    { // l'elemento va inserito in testa
        Node_t *node = NULL;
        CHECK_EQ_EXIT(malloc, (node = (Node_t *)malloc(sizeof(Node_t))), NULL, "malloc", "");
        node->data = msg;
        node->next = coda->head->next;
        coda->head->next = node;
        return coda;
    }
    else
    {

        Node_t *prec = NULL;
        Node_t *corr = coda->head->next;
        int trovato = 0;
        while (corr != NULL && !trovato)
        {
            msg_t *msg_corr = corr->data;
            if (msg->res <= msg_corr->res)
            {
                trovato = 1;
            }
            else
            {
                prec = corr;
                corr = corr->next;
            }
        }
        if (corr == NULL)
        {
            push(coda, (msg_t *)msg);
            return coda;
        }
        else
        {
            Node_t *node = NULL;
            CHECK_EQ_EXIT(malloc, (node = (Node_t *)malloc(sizeof(Node_t))), NULL, "malloc", "");
            node->data = msg;
            prec->next = node;
            node->next = corr;
            coda->qlen += 1;
            return coda;
        }
    }

    push(coda, (msg_t *)msg);

    return coda;
}


//funzione di gestione della richiesta da parte di un client già connesso
int cmd(int connfd, Queue_t* coda){
    int len=-1;
    char *buffer = NULL;
    msg_t *m = NULL;

    if ((readn(connfd, &len, sizeof(int))) == -1)
    {
        perror("read1");
        return -1;
    }

    if (len <= 0)
        return -1;

    buffer = (char*)calloc(len,sizeof(char));

    if (!buffer)
    {
        free(buffer);
        perror("calloc");
        return -1;
    }
    int k;
    if ((k = readn(connfd, buffer, len * sizeof(char))) == -1)
    {
        free(buffer);
        perror("read2");
        return -1;
    }

    if (buffer != NULL)
    {
        m = (msg_t *)malloc(sizeof(msg_t));

        if(m == NULL){
            perror("malloc");
            return -1;
        }

        m->str = (char *)malloc(sizeof(char) * (len + 1)); 
        
        if(m->str == NULL){
            perror("malloc");
            return -1;        
        }

        strcpy(m->str, buffer);
        m->str[len] = '\0';

        m->len = len;
        char *tmp = (char *)malloc(sizeof(char) * len); 
        
        if(tmp == NULL){
            perror("malloc");
            return -1;
        }

        strncpy(tmp, m->str, len);
        char *res1 = NULL;

        res1 = strtok(tmp, " ");

        if (res1 != NULL){

            long res2 = strtol(res1, NULL, 10);

            m->res = res2;

            coda = pushOrdered(coda, (msg_t *)m);
            
            if(tmp) free(tmp);
        }

    }
    if(buffer) free(buffer);
    return 0;
}