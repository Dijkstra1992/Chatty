/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 * @file connections.c
 * @brief Implementazione dell'interfaccia connections.h
 * @author Pietro Scarso 544175
 *
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera originale
 * dell'autore
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include <connections.h>
#include <message.h>

/* Legge dataSize bytes dal socket fd e li scrive sul buffer data */
int readFromSock(long fd, void *data, size_t dataSize) {
    size_t r = 0, l = 0;
    while(r < dataSize) {
        if ((l = read(fd, (char *)data + r, dataSize - r)) <= 0){
            return l;
        }
        r += l;
    }
    return 1;
}

/* Scrive dataSize byte dal buffer data al socket fd */
int writeToSock(long fd, void *data, size_t dataSize) {
    size_t r = 0, l = 0;
    while(r < dataSize) {
        if ((l = write(fd, (char *)data + r, dataSize - r)) <= 0){
            return l;
        }
        r += l;

    }
    return 1;
}

/* Apre una connessione AF_UNIX verso il server */
int openConnection(char* path, unsigned int ntimes, unsigned int secs) {
    int fd;
    int estabilished = 0;
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    int i = 1;
    while (i <= ntimes && estabilished == 0) {
        if (connect(fd, &addr, sizeof(addr)) == -1) {
            if (errno == ETIMEDOUT) {
              printf("Connection tryout[%d]...\n", i);
              sleep(secs);
              i++;
            }
            else {
              perror("Connecting to server");
              return -1;
            }
        }
        else { estabilished = 1; printf("Connection estabilished!\n"); }
    }
    if(estabilished == 0) {
      return -1;
    }

    return fd;
}

/* Legge l'header del messaggio */
int readHeader(long fd, message_hdr_t *hdr){

    size_t dataSize = sizeof(message_hdr_t);

    return readFromSock(fd, hdr, dataSize);
}

/* Legge il body del messaggio */
int readData(long fd, message_data_t *data) {

    size_t dataSize = sizeof(message_data_hdr_t);
    int r;
    if ((r = readFromSock(fd, &(data->hdr), dataSize)) <= 0) {
        return r;
    }
    dataSize = data->hdr.len;
    data->buf = calloc(dataSize, 1);
    if ((r = readFromSock(fd, data->buf, dataSize)) <= 0) {
        return r;
    }
    return 1;
}

/* Legge l'intero messaggio */
int readMsg(long fd, message_t *msg) {
    int hd;

    hd = readHeader(fd, &(msg->hdr));
    if (hd <= 0) {
        return hd;
    }
    return readData(fd, &(msg->data));
}

/* Invia un messaggio di tipo message_hdr_t */
int sendHeader(long fd, message_hdr_t *hdr) {

    size_t dataSize = sizeof(message_hdr_t);

    return writeToSock(fd, hdr, dataSize);
}

/* Invia un messaggio di richiesta al server (client side) */
int sendRequest(long fd, message_t *msg) {

    if (sendHeader(fd, &(msg->hdr)) < 0) {
        return -1;
    }
    if (sendData(fd, &(msg->data)) < 0) {
        return -1;
    }

    return 1;
}

/* Invia il body del messaggio */
int sendData(long fd, message_data_t *msg) {

    if (writeToSock(fd, &(msg->hdr), sizeof(message_hdr_t)) < 0) {
        return -1;
    }
    size_t dataSize = (size_t)msg->hdr.len;
    if (writeToSock(fd, msg->buf, dataSize) < 0) {
        return -1;
    }

    return 1;
}
