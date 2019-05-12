/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 *
 *
 * @file history.c
 * @brief Implementazione dell'interfaccia history.h
 * @author Pietro Scarso 544175
 *
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera originale
 * dell'autore
 */

 #include <stdlib.h>
 #include <string.h>
 #include <history.h>

msg_history_t *initHistory(size_t max_msgs) {
    msg_history_t *hst = malloc(sizeof(msg_history_t));
    hst->msgs = malloc(sizeof(message_t *) * max_msgs);
    if (!hst->msgs) return NULL;
    hst->first = 0;
    hst->last = 0;
    hst->msgcount = 0;
    hst->maxmsgs = max_msgs;
    hst->next = 0;
    return hst;
}

int addMsg(msg_history_t *hst, message_t *msg) {
    if (hst->msgcount < hst->maxmsgs) { //slot vuoto
        hst->msgs[hst->next] =  malloc(sizeof(msg));
        if (!hst->msgs[hst->next]) return -1;
        hst->msgs[hst->next] =  msg;
        hst->last = hst->next;
        hst->msgcount++;
        hst->next = (hst->next +1) % (hst->maxmsgs);
    }
    else { //il buffer è pieno, sovrascrivo il messaggio  più vecchio
        free(hst->msgs[hst->next]);
        hst->msgs[hst->next] = malloc(sizeof(message_t));
        if (!hst->msgs[hst->next]) return -1;
        hst->msgs[hst->next] = msg;
        hst->last = hst->next;
        hst->first =(hst->first +1)%(hst->maxmsgs);
        hst->next = hst->first;
    }
    return 0;
}

void destroyHistory(msg_history_t *hst) {
    for (int s = 0; s < hst->maxmsgs; s++) {
        if (hst->msgs[s]) free(hst->msgs[s]);
    }
    free(hst);
}
