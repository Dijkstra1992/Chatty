#ifndef QUEUE_H_
#define QUEUE_H_

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
    Node_t        *head;
    Node_t        *tail;
    unsigned long  qlen;
} Queue_t;


/** Alloca ed inizializza una coda. Deve essere chiamata da un solo
 *  thread (tipicamente il thread main).
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval q puntatore alla coda allocata
 */
Queue_t *initQueue();

/** Cancella una coda allocata con initQueue. Deve essere chiamata da
 *  da un solo thread (tipicamente il thread main).
 *
 *   \param q puntatore alla coda da cancellare
 */
void deleteQueue(Queue_t *q);

/** Cerca il filedescriptor data nella coda q, ritorna 1 se lo trova, -1 altrimenti */
int onlineFd(Queue_t *q, long data);

/** Cerca il nickname data nella coda q, ritorna 1 se lo trova, -1 altrimenti */
int onlineNick(Queue_t *q, char *data);

/** Ritorna il filedescriptor del socket dell'utente nick, -1 in caso di fallimento */
long getFd(Queue_t *q, char *data);

/** Elimina l'elemento data dalla coda q, ritorna 1 se successo, -1 altrimenti */
int deleteNode(Queue_t *q, long data);

/** Inserisce un dato nella coda.
 *   \param data puntatore al dato da inserire
 *
 *   \retval 0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int push(Queue_t *q, void *data);

/** Estrae un dato dalla coda.
 *
 *  \retval data puntatore al dato estratto.
 */
void *pop(Queue_t *q);

/** Esegue il dump dell'intera lista q in un buffer
 *
 *  \param q puntatore alla coda
 *
 *  \retval buf   puntatore al buffer creato se successo
 *  \retval NULL  in caso di errore
 */
int dumpList(Queue_t *q, char *buf);

unsigned long length(Queue_t *q);

#endif /* QUEUE_H_ */
