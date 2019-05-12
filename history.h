/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 *
 *
 * @file history.h
 * @brief Interfaccia che definisce la struttura dati e le fuzioni utilizzate per la
 *        gestione della history dei messaggi
 * @author Pietro Scarso 544175
 *
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera originale
 * dell'autore
 */
#ifndef HISTORY_H
#define HISTORY_H

#include <message.h>

// struttura dati che rappresenta la history dei messaggi
typedef struct _msg_history_t {
    int first;     //indice del primo messaggio
    int last;      //indice dell'ultimo messaggio
    int next;      //indice del successivo slot libero
    int msgcount;  //numero di messaggi ricevuti
    int maxmsgs;   //capienza massima della history
    message_t **msgs;   //buffer dei messaggi
} msg_history_t;

/** @function initHistory
  * @brief  funzione che inizializza la struttura dati msg_history_t
  *
  * @param  max_msgs  numero massimo di messaggi che dovranno essere memorizzati
  *
  * @return hst   puntatore alla struttura dati creata, NULL in caso di fallimento
  */
msg_history_t *initHistory(size_t max_msgs);

/** @function addMsg
  * @brief  funzione che salva un nuovo message_t nella history
  *
  * @param  hst   puntatore alla history
  * @param  msg   puntatore al messaggio di tipo message_t da salvare
  *
  * @return   0 se successo, -1 altrimenti
  *
  */
int addMsg(msg_history_t *hst, message_t *msg);

/** @function destroyHistory
  * @brief  funzione che elimina la struttura dati e libera la memoria
  *
  * @param  hst   puntatore alla struttura da eliminare
  *
  */
void destroyHistory(msg_history_t *hst);

#endif
