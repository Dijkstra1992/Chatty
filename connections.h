/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <message.h>

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo di comunicazione
 *        tra i clients ed il server
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

// -------- server side -----
/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */

 /*
  * @funtion  readFromSock
  * @brief    legge dataSize byte dal socket fd e li scrive su data
  *
  * @param  fd    filedescriptor del socket dal quale leggere
  * @param  data  puntatore al buffer sul quale scrivere i dati
  * @param  dataSize quantità di byte da leggere
  *
  * @return < 0 se c'è stato un errore, > 0 altrimenti
  */
int readFromSock(long fd, void *data, size_t dataSize);

/*
 * @funtion  writeToSock
 * @brief    scrive dataSize byte, dal buffer data, al socket fd
 *
 * @param  fd    filedescriptor del socket sul quale scrivere
 * @param  data  puntatore al buffer dal quale quale trasferire i dati
 * @param  dataSize quantità di byte da scrivere
 *
 * @return < 0 se c'è stato un errore, > 0 altrimenti
 */
int writeToSock(long fd, void *data, size_t dataSize);

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readHeader(long connfd, message_hdr_t *hdr);

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data);

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <= 0 se c'è stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg);

/* da completar.e da parte dello studente con altri metodi di interfaccia */

/*
 *  @function sendHeader
 *  @brief  invia un messagio di tipo message_data_hdr_t
 *
 *  @param  fd  descrittore della connessione
 *  @param  hdr puntatore al messaggio da inviare
 *
 *  @return <= 0 se c'è stato un errore (se < 0 setta errno, se == 0 connessione chiusa)
 */
int sendHeader(long fd, message_hdr_t *hdr);

// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg);

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg);


/* da completare da parte dello studente con eventuali altri metodi di interfaccia */


#endif /* CONNECTIONS_H_ */
