/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 *
 *
 * @file chatty.c
 * @brief File principale del server chatterbox
 * @author Pietro Scarso 544175
 *
 * Si dichiara che il contenuto di questo file è in ogni sua parte opera originale
 * dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <config.h>
#include <message.h>
#include <connections.h>
#include <ops.h>
#include <stats.h>
#include <icl_hash.h>
#include <queue.h>
#include <history.h>

/******************************************************************************
              Variabili e strutture dati globali
 *****************************************************************************/
/* struttura dati che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

static config_st config;  //struttura dati contenente i valori per la configurazione del server,
                          //definita in config.h

static fd_set fds;      //set sul quale il server resta in ascolto per connessioni in ingresso

/* struttura dati nella quale verranno registrati i client */
icl_hash_t *usersDB;

/* coda che conterrà i fd dei client che vogliono trasmettere */
Queue_t *task_queue;

/* coda che conterrà l'elenco dei client online */
Queue_t *online_queue;

/* variabili di condizione e mutua escluzione */
static pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;          //  Per la mutua esclusione su activeConns
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;         //  Per accedere in mutua esclusione al usersDB degli utenti registrati
static pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;          //  Per accedere in mutua esclusione alla coda dei task
static pthread_mutex_t online_mutex = PTHREAD_MUTEX_INITIALIZER;        //  Per accedere in mutua esclusione alla coda dei client online
static pthread_cond_t onlineUsers = PTHREAD_COND_INITIALIZER;           //  Per l'attesa dei thread workers quando il numero di client connessi è == 0

/* variabile che indica lo stato del server:
      se 1 ==> server avviato
      se -1 ==> server spento
*/
static int running = -1;

/* contatore degli utenti attivi */
static int activeConns = 0;

/*  **********************************************************
                Funzioni di utilità
  ********************************************************** */

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/* macro utilizzata per il parsing del file di configurazione */
#define nextLine() {char c;  while((c = fgetc(fp))) { if (c == '\n' || c == EOF) break; }}

/*  funzione che legge i dati di configurazione dal file puntato dal parametro path,
 *  e li inserisce nella struttura dati config
 */
static void serverConfig(const char *path) {
    printf("Extracting configuration values from file [%s] ...\n", path);
    /* apertura file di configurazione ed estrazione valori */
    FILE *fp;
    if ((fp = fopen(path, "r")) == NULL) {
        perror("Opening configuration file");
        exit(EXIT_FAILURE);
    }
    char arg[MAX_NAME_LENGTH] = {'\0'};
	  char value[MAX_NAME_LENGTH] = {'\0'};
    int val_len;

	   while ( !feof(fp) ) {
		     fscanf(fp, "%s =", arg);
		     if ( arg[0] == '#') {
			      nextLine();
			      continue;
		     }
         if (strcmp(arg, "UnixPath") == 0) {
            fscanf(fp, "%s\n", value);
            val_len = strlen(value);
            config.UnixPath = malloc(val_len * sizeof(char));
            strncpy(config.UnixPath, value, val_len);
            printf("UnixPath:\t %s\n", config.UnixPath);
         }
         else if (strcmp(arg, "DirName") == 0) {
            fscanf(fp, "%s\n", value);
            val_len = strlen(value);
            config.DirName = malloc(val_len * sizeof(char));
            strncpy(config.DirName, value, val_len);
            printf("DirName:\t %s\n", config.DirName);
         }
         else if (strcmp(arg, "StatFileName") == 0) {
            fscanf(fp, "%s\n", value);
            val_len = strlen(value);
            config.StatFileName = malloc(val_len * sizeof(char));
            strncpy(config.StatFileName, value, val_len);
            printf("StatFileName:\t %s\n", config.StatFileName);
         }
         else if (strcmp(arg, "MaxConnections") == 0) {
            fscanf(fp, "%s\n", value);
            config.MaxConnections = atoi(value);
            printf("MaxConnections:\t %d\n", config.MaxConnections);
         }
         else if (strcmp(arg, "MaxMsgSize") == 0) {
            fscanf(fp, "%s\n", value);
            config.MaxMsgSize = atoi(value);
            printf("MaxMsgSize:\t %d\n", config.MaxMsgSize);
         }
         else if (strcmp(arg, "ThreadsInPool") == 0) {
            fscanf(fp, "%s\n", value);
            config.ThreadsInPool = atoi(value);
            printf("ThreadsInPool:\t %d\n", config.ThreadsInPool);
         }
         else if (strcmp(arg, "MaxFileSize") == 0) {
            fscanf(fp, "%s\n", value);
            config.MaxFileSize = atoi(value);
            printf("MaxFileSize:\t %d\n", config.MaxFileSize);
         }
         else if (strcmp(arg, "MaxHistMsgs") == 0) {
            fscanf(fp, "%s\n", value);
            config.MaxHistMsgs = atoi(value);
            printf("MaxHistMsgs:\t %d\n", config.MaxHistMsgs);
         }
         //printf("%s = %s\n", arg, value);
	   }
     fclose(fp);
     printf("\nServer configuration completed!\n");

}

/*
 ******************************************************
                Signal Handler
 ******************************************************
 */
static void sigHandler(int signum) {

    switch(signum) {

        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
                write(1, "\nReceived shutdown signal\n", 26);
                running = -1;
            break;

        case SIGUSR1:
                if (running < 0) ;
                else {
                    write(1, "Received stats signal\n", 22);
                    FILE *fout = fopen(config.StatFileName, "a");
                    printStats(fout);
                    fclose(fout);
                }
            break;

        default: break;
    }
}


/*
 ******************************************************
                Worker Threads function
 ******************************************************
 */
static void *wexec() {

    int registered; //  variabile di controllo che indica se l'utente è registrato
    int online;     //  variabile di controllo che indica se l'utente è online
    while(running > 0) {
        // se non ci sono task in coda mi metto in attesa
        pthread_mutex_lock(&task_mutex);
        while (length(task_queue) == 0) {
            if (running < 0) goto quit;
            else if (pthread_cond_wait(&onlineUsers, &task_mutex) != 0) perror("pthread_cond_wait()\n");
        }
        pthread_mutex_unlock(&task_mutex);

        // estraggo il primo fd dalla coda dei task
        pthread_mutex_unlock(&task_mutex);
        long cfd = (long) pop(task_queue);
        pthread_mutex_unlock(&task_mutex);

        message_t request;
        memset(&request, 0, sizeof(message_t));

        // leggo il messaggio di richiesta del client
        int r = readMsg(cfd, &request);
        if (r <= 0) {
            if (r < 0) { perror("readMsg"); chattyStats.nerrors++; }
            else {
                pthread_mutex_lock(&online_mutex);
                if (length(online_queue) > 0) {
                    pthread_mutex_unlock(&online_mutex);
                    if (onlineFd(online_queue, cfd) == 1) {
                        if(deleteNode(online_queue, cfd) == 1) {
                            printf("User_id %ld went offline\n", cfd);
                            chattyStats.nonline--;
                            pthread_mutex_lock(&conn_mutex);
                            activeConns--;
                            printf("[THREAD]: active connections: %d\n", activeConns);
                            pthread_mutex_unlock(&conn_mutex);
                            close(cfd);
                        }
                        else { printf("error disconnecting user!\n"); chattyStats.nerrors++; }
                    }
                }
            }
        }
        else {
            // inizializzo le strutture utilizzate per rispondere al client
            message_data_t replyData;
            message_hdr_t replyHdr;
            memset(&replyHdr, 0, sizeof(message_hdr_t));
            memset(&replyData, 0, sizeof(message_data_t));

            // prima di elaborare la richiesta verifico che il nickname sia lecito
            if (strlen(request.hdr.sender) > MAX_NAME_LENGTH) {
                setHeader(&replyHdr, OP_FAIL, "nickname too long");
                sendHeader(cfd, &replyHdr);
                chattyStats.nerrors++;
                break;
            }
            // verifico se l'utente è già registrato
            else {
                pthread_mutex_lock(&users_mutex);
                registered = icl_hash_find(usersDB, (char *)request.hdr.sender);
                printf("-----------------------------------\n"
                "[Thread_id %ld]:\n Current request [%d] from user [%s], status: %d\n", pthread_self(), request.hdr.op, request.hdr.sender, registered);
                pthread_mutex_unlock(&users_mutex);
            }

            // elaboro i vari tipi di richieste
            switch (request.hdr.op) {

                case REGISTER_OP:
                        if (registered) { //se l'utente è già registrato rispondo OP_NICK_ALREADY
                            setHeader(&replyHdr, OP_NICK_ALREADY, "User is already registered");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }

                        else {  //altrimenti procedo con la registrazione
                            char *nick = malloc(MAX_NAME_LENGTH+1);
                            strncpy(nick, request.hdr.sender, MAX_NAME_LENGTH);
                            msg_history_t *history = initHistory(config.MaxHistMsgs);
                            pthread_mutex_lock(&users_mutex);
                            if (icl_hash_insert(usersDB, nick, history) == NULL) { //inserimento fallito
                                pthread_mutex_unlock(&users_mutex);
                                setHeader(&replyHdr, OP_FAIL, "Registration failed");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                            else {  //inserimento riuscito, rispondo con OP_OK e invio la lista degli utenti online
                                pthread_mutex_unlock(&users_mutex);
                                chattyStats.nusers++;
                                pthread_mutex_lock(&online_mutex);
                                // aggiungo l'utente alla lista di utenti online
                                user_t *user = malloc(sizeof(user_t));
                                user->nick = malloc(MAX_NAME_LENGTH+1);
                                strncpy(user->nick, (char *)request.hdr.sender, MAX_NAME_LENGTH);
                                memset(&user->sock, 0, sizeof(long));
                                user->sock = (long) cfd;

                                push(online_queue, user);
                                chattyStats.nonline++;

                                int nusers = length(online_queue);
                                size_t size = (nusers * (MAX_NAME_LENGTH+1));
                                replyData.buf = malloc(size);

                                if (dumpList(online_queue, replyData.buf) == -1) {
                                    setHeader(&replyHdr, OP_FAIL, "Unable to load online users list");
                                    sendHeader(cfd, &replyHdr);
                                    pthread_mutex_unlock(&online_mutex);
                                    break;
                                }
                                printf("Lista utenti online [%d]:\n", nusers);
                              	for(int i=0,p=0;i<nusers; ++i, p+=(MAX_NAME_LENGTH+1)) {
                              	    printf("- %s\n", &replyData.buf[p]);
                              	}
                                pthread_mutex_unlock(&online_mutex);
                                // invio l'ack al client
                                setHeader(&replyHdr, OP_OK, "OK");
                                sendHeader(cfd, &replyHdr);
                                // invio la parte dati con la lista degli utenti online
                                setData(&replyData, (char *)request.hdr.sender, (char *)replyData.buf, size);
                                if (sendData(cfd, &replyData) <= 0) {
                                    printf("Failed sending user list\n");
                                    break;
                                }
                            }
                        }
                        printf("REGISTER_OP COMPLETED!\n\n");
                    break;

                case UNREGISTER_OP:

                        if(!registered) {
                            setHeader(&replyHdr, OP_FAIL, "User is not registered");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                            break;
                        }
                        else {
                            pthread_mutex_lock(&users_mutex);
                            if (icl_hash_delete(usersDB, request.hdr.sender, free, destroyHistory) == -1) {
                                setHeader(&replyHdr, OP_FAIL, "Error deleting user");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                            else {
                                setHeader(&replyHdr, OP_OK, "User deleted");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nusers--;
                            }
                            pthread_mutex_unlock(&users_mutex);
                        }
                        printf("UNREGISTER_OP COMPLETED!\n");
                    break;

                case CONNECT_OP:
                        pthread_mutex_lock(&online_mutex);
                        if(!registered) { //se l'utente non è registrato, invio un messaggio OP_NICK_UNKNOWN
                            pthread_mutex_unlock(&online_mutex);
                            setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        // l'utente è già online
                        else if (onlineNick(online_queue, request.hdr.sender) == 1) {
                            pthread_mutex_unlock(&online_mutex);
                            printf("User already online\n");
                            setHeader(&replyHdr, OP_NICK_ALREADY, "User already logged in");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }

                        else {  //se l'utente è registrato ma non è online, devo connetterlo
                            user_t *user = malloc(sizeof(user_t));
                            user->nick = malloc(MAX_NAME_LENGTH+1);
                            user->sock = cfd;
                            strncpy(user->nick, (char *)request.hdr.sender, MAX_NAME_LENGTH);
                            push(online_queue, user);
                            chattyStats.nonline++;

                            //preparo la lista di utenti online
                            int nusers = length(online_queue);
                            size_t size = (nusers * (MAX_NAME_LENGTH+1));
                            replyData.buf = malloc(size);

                            if (dumpList(online_queue, replyData.buf) == -1) {
                                setHeader(&replyHdr, OP_FAIL, "Unable to load online users list");
                                sendHeader(cfd, &replyHdr);
                                pthread_mutex_unlock(&online_mutex);
                                chattyStats.nerrors++;
                                break;
                            }
                            printf("Lista utenti online [%d]:\n", nusers);
                            for(int i=0,p=0;i<nusers; ++i, p+=(MAX_NAME_LENGTH+1)) {
                                printf("- %s\n", &replyData.buf[p]);
                            }
                            pthread_mutex_unlock(&online_mutex);
                            // invio l'ack al client
                            setHeader(&replyHdr, OP_OK, "OK");
                            sendHeader(cfd, &replyHdr);
                            // invio la parte dati con la lista degli utenti online
                            setData(&replyData, (char *)request.hdr.sender, (char *)replyData.buf, size);
                            if (sendData(cfd, &replyData) <= 0) {
                                printf("Failed sending user list\n");
                                chattyStats.nerrors++;
                                break;
                            }
                        }
                        printf("CONNECT_OP COMPLETED!\n\n");
                    break;

                case USRLIST_OP:
                        if (onlineNick(online_queue, request.hdr.sender) == -1) {
                            setHeader(&replyHdr, OP_FAIL, "user not logged in");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        else {
                            pthread_mutex_lock(&online_mutex);
                            int nusers = length(online_queue);
                            size_t size = (nusers * (MAX_NAME_LENGTH+1));
                            replyData.buf = malloc(size);

                            if (dumpList(online_queue, replyData.buf) == -1) {
                                pthread_mutex_unlock(&online_mutex);
                                setHeader(&replyHdr, OP_FAIL, "Unable to load online users list");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                            else {
                                pthread_mutex_unlock(&online_mutex);
                                printf("Lista utenti online [%d]:\n", nusers);
                                for(int i=0,p=0;i<nusers; ++i, p+=(MAX_NAME_LENGTH+1)) {
                                    printf("- %s\n", &replyData.buf[p]);
                                }
                                // invio l'ack al client
                                setHeader(&replyHdr, OP_OK, "OK");
                                sendHeader(cfd, &replyHdr);
                                // invio la parte dati con la lista degli utenti online
                                setData(&replyData, (char *)request.hdr.sender, (char *)replyData.buf, size);
                                if (sendData(cfd, &replyData) <= 0) {
                                    printf("Failed sending user list\n");
                                    chattyStats.nerrors++;
                                }
                            }

                        }

                        printf("USRLIST_OP COMPLETED!\n");
                    break;

                case POSTTXT_OP:
                         //se l'utente non è registrato, invio un messaggio OP_FAIL
                        if (!registered) {
                            setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        // l'utente non è online, non può inviare il messaggio
                        else if (!onlineNick(online_queue, request.hdr.sender)) {
                            setHeader(&replyHdr, OP_FAIL, "User not logged in");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        else {
                            // verifico che il destinatario sia un utente registrato
                            pthread_mutex_lock(&users_mutex);
                            int found = icl_hash_find(usersDB, (char *)request.data.hdr.receiver);
                            pthread_mutex_unlock(&users_mutex);

                            if (!found) { //il destinatario non è un utente registrato, rispondo con OP_NICK_UNKNOWN
                                setHeader(&replyHdr, OP_NICK_UNKNOWN, "Unknown receiver");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                            else if (request.data.hdr.len <= config.MaxMsgSize){ //altrimenti verifico che il messaggio non sia troppo grande
                                char *msg = malloc(config.MaxMsgSize);
                                char *rcv_nick = malloc(MAX_NAME_LENGTH+1);
                                strncpy(msg, request.data.buf, config.MaxMsgSize);
                                strncpy(rcv_nick, request.data.hdr.receiver, MAX_NAME_LENGTH);
                                printf("POSTTXT_OP:\n");
                                printf(" -Sender    :[%s]\n", request.hdr.sender);
                                printf(" -Receiver  :[%s]\n", rcv_nick);
                                printf(" -Message   :[%s]\n", msg);

                                pthread_mutex_lock(&online_mutex);
                                if (online = onlineNick(online_queue, rcv_nick) == 1) { //il destinatario è online, invio una notifica e poi il messaggio
                                    printf("User is online, sending notification...\n");
                                    long rcv_sock = getFd(online_queue, rcv_nick);
                                    pthread_mutex_unlock(&online_mutex);

                                    setHeader(&replyHdr, TXT_MESSAGE, "New message to read");
                                    sendHeader(rcv_sock, &replyHdr);

                                    setData(&replyData, rcv_nick, msg, config.MaxMsgSize);
                                    sendData(rcv_sock, &replyData);
                                    chattyStats.ndelivered++;
                                }
                                pthread_mutex_unlock(&online_mutex);
                                // salvo il messaggio nella msg_history del destinatario
                                message_t *new_msg = malloc(sizeof(message_t));
                                (new_msg->data).buf = malloc(config.MaxMsgSize);
                                setHeader(&new_msg->hdr, TXT_MESSAGE, request.hdr.sender);
                                setData(&new_msg->data, rcv_nick, msg, config.MaxMsgSize);

                                pthread_mutex_lock(&users_mutex);
                                int delivered = addMsg((msg_history_t *)icl_hash_get(usersDB, rcv_nick), new_msg);
                                pthread_mutex_unlock(&users_mutex);
                                if (delivered == -1) {//operazione fallita
                                    printf("ERROR: saving message on %ss history!\n", rcv_nick);
                                    setHeader(&replyHdr, OP_FAIL, "delivering message");
                                    sendHeader(cfd, &replyHdr);
                                    chattyStats.nerrors++;
                                }
                                else {
                                  //invio l'ack di conferma al mittente del messaggio
                                  setHeader(&replyHdr, OP_OK, "Message send");
                                  sendHeader(cfd, &replyHdr);
                                  if (online == -1) chattyStats.nnotdelivered++;
                                }
                            }
                            else {//il messaggio non rispetta i parametri
                                setHeader(&replyHdr, OP_MSG_TOOLONG, "Message is too long");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                        }
                        printf("POSTTXT_OP COMPLETED!\n");
                    break;

                case POSTTXTALL_OP:
                      //se l'utente non è registrato, invio un messaggio OP_FAIL
                      if (!registered) {
                          setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      // l'utente non è online, non può inviare il messaggio
                      else if (!onlineNick(online_queue, request.hdr.sender)) {
                          setHeader(&replyHdr, OP_FAIL, "User not logged in");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      else if (request.data.hdr.len > config.MaxMsgSize){
                          setHeader(&replyHdr, OP_MSG_TOOLONG, "message is too long");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      else { //preparo il messaggio da consegnare
                          char *msg = malloc(config.MaxMsgSize);
                          char *rcv_nick = malloc(MAX_NAME_LENGTH+1);
                          strncpy(msg, request.data.buf, config.MaxMsgSize);
                          strncpy(rcv_nick, request.data.hdr.receiver, MAX_NAME_LENGTH);
                          printf("POSTTXTALL_OP:\n");
                          printf(" -Sender    :[%s]\n", request.hdr.sender);
                          printf(" -Message   :[%s]\n", msg);
                          message_t *new_msg = malloc(sizeof(message_t));
                          (new_msg->data).buf = malloc(config.MaxMsgSize);
                          setHeader(&new_msg->hdr, TXT_MESSAGE, request.hdr.sender);
                          setData(&new_msg->data, rcv_nick, msg, config.MaxMsgSize);
                          //scorro l'intera tabella di utenti registrati e aggiungo il messaggio alla loro history
                          int i;
                          char *key;
                          icl_entry_t *ent;
                          msg_history_t *value;
                          pthread_mutex_lock(&users_mutex);
                          printf("Posting message to: \n");
                          icl_hash_foreach(usersDB, i, ent, key, value) {
                              printf("  - %s\n", key);
                              if (strcmp(key, request.hdr.sender) != 0) {
                                  pthread_mutex_lock(&online_mutex);
                                  online = onlineNick(online_queue, key);
                                  if (online == 1) {
                                      printf("User is online, sending notification...\n");
                                      long rcv_sock = getFd(online_queue, rcv_nick);
                                      pthread_mutex_unlock(&online_mutex);
                                      setHeader(&replyHdr, TXT_MESSAGE, "New message to read");
                                      sendHeader(rcv_sock, &replyHdr);
                                      setData(&replyData, rcv_nick, msg, config.MaxMsgSize);
                                      sendData(rcv_sock, &replyData);
                                      chattyStats.ndelivered++;
                                  }
                                  else {
                                      pthread_mutex_unlock(&online_mutex);
                                      if (addMsg(value, new_msg) == -1) {
                                          printf("ERROR: couldn't send message to %s\n", key);
                                          chattyStats.nerrors++;
                                      }
                                      else chattyStats.nnotdelivered++;
                                  }
                              }
                          }
                      }
                      pthread_mutex_unlock(&users_mutex);
                      setHeader(&replyHdr, OP_OK, "");
                      sendHeader(cfd, &replyHdr);
                      printf("POSTTXTALL_OP COMPLETED!\n");
                  break;

                case POSTFILE_OP:
                        //se l'utente non è registrato, invio un messaggio OP_FAIL
                        if (!registered) {
                            setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        // l'utente non è online, non può inviare il file
                        else if (!onlineNick(online_queue, request.hdr.sender)) {
                            setHeader(&replyHdr, OP_FAIL, "User not logged in");
                            sendHeader(cfd, &replyHdr);
                            chattyStats.nerrors++;
                        }
                        else {
                            // verifico che il destinatario sia un utente registrato
                            char *rcv_nick =malloc(MAX_NAME_LENGTH+1);
                            strncpy(rcv_nick, request.data.hdr.receiver, MAX_NAME_LENGTH);
                            pthread_mutex_lock(&users_mutex);
                            int found = icl_hash_find(usersDB, (char *)rcv_nick);
                            pthread_mutex_unlock(&users_mutex);

                            if (!found) { //il destinatario non è un utente registrato, rispondo con OP_NICK_UNKNOWN
                                setHeader(&replyHdr, OP_NICK_UNKNOWN, "Unknown receiver");
                                sendHeader(cfd, &replyHdr);
                                chattyStats.nerrors++;
                            }
                            else {
                                message_data_t file;
                                if (readData(cfd, &file) <= 0) {
                                    setHeader(&replyHdr, OP_FAIL, "reading file data");
                                    sendHeader(cfd, &replyHdr);
                                    chattyStats.nerrors++;
                                }
                                else if (file.hdr.len > config.MaxFileSize){ //file troppo grande
                                    printf("File size: %d, Max File size: %d\n", file.hdr.len, config.MaxFileSize);
                                    setHeader(&replyHdr, OP_MSG_TOOLONG, "File is too big");
                                    sendHeader(cfd, &replyHdr);
                                    chattyStats.nerrors++;
                                }
                                else {
                                    //scarico il file nella cartella indicata in config.DirName
                                    int fp;
                                    char *path = malloc(strlen(config.DirName) + request.data.hdr.len + 2);
                                    char *dest;
                                    strcat(path, config.DirName);
                                    strcat(path, "/");
                                    strcat(path, request.data.buf);
                                    printf("Path of new file: %s\n", path);
                                    fp = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                                    ftruncate(fp, file.hdr.len);
                                    dest = mmap(NULL, file.hdr.len, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
                                    memcpy(dest, file.buf, file.hdr.len);
                                    close(fp);
                                    //preparo il messaggio da aggiungere alla history
                                    message_t *new_msg = malloc(sizeof(message_t));
                                    (new_msg->data).buf = malloc(config.MaxMsgSize);
                                    setHeader(&new_msg->hdr, FILE_MESSAGE, request.hdr.sender);
                                    setData(&new_msg->data, rcv_nick, request.data.buf, file.hdr.len);
                                    //aggiungo il messaggio alla history
                                    pthread_mutex_lock(&users_mutex);
                                    int delivered = addMsg((msg_history_t *)icl_hash_get(usersDB, rcv_nick), new_msg);
                                    pthread_mutex_unlock(&users_mutex);
                                    if (delivered == -1) {//operazione fallita
                                        printf("ERROR: saving message on %ss history!\n", rcv_nick);
                                        setHeader(&replyHdr, OP_FAIL, "delivering message");
                                        sendHeader(cfd, &replyHdr);
                                        chattyStats.nerrors++;
                                    }
                                    else {
                                      //invio l'ack di conferma al mittente del messaggio
                                      setHeader(&replyHdr, OP_OK, "Message send");
                                      sendHeader(cfd, &replyHdr);
                                      chattyStats.nfilenotdelivered++;
                                    }
                                }
                            }
                        }
                        printf("POSTFILE_OP COMPLETED!\n");
                    break;

                case GETPREVMSGS_OP:
                          //se l'utente non è registrato, invio un messaggio OP_FAIL
                          if (!registered) {
                              setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                              sendHeader(cfd, &replyHdr);
                              chattyStats.nerrors++;
                          }
                          // l'utente non è online, non può scaricare la history
                          else if (!onlineNick(online_queue, request.hdr.sender)) {
                              setHeader(&replyHdr, OP_FAIL, "User not logged in");
                              sendHeader(cfd, &replyHdr);
                              chattyStats.nerrors++;
                          }
                          else {//estraggo la sua history
                              pthread_mutex_lock(&users_mutex);
                              msg_history_t *history = (msg_history_t *)icl_hash_get(usersDB, request.hdr.sender);
                              setHeader(&replyHdr, OP_OK, "");
                              sendHeader(cfd, &replyHdr);
                              size_t n = history->msgcount;
                              setData(&replyData, "", (char *)&n, sizeof(size_t));
                              sendData(cfd, &replyData);
                              printf("Messages to send: %ld\n", n);
                              int idx = history->first;
                              int count = 1;
                              while (count <= n) {
                                  message_t *tmp = (message_t *)history->msgs[idx];
                                  printf("Message #%d: \n", count);
                                  printf(" - Message: %s\n", (char *)(tmp->data).buf);
                                  printf(" - Type:    %d\n", tmp->hdr.op);
                                  printf(" - Sender:  %s\n", (char *)(tmp->hdr).sender);
                                  sendRequest(cfd, (message_t *)tmp);
                                  idx = (idx+1)%history->maxmsgs;
                                  count++;
                                  if (tmp->hdr.op == TXT_MESSAGE) chattyStats.ndelivered++;
                                  else chattyStats.nfiledelivered++;
                              }
                              pthread_mutex_unlock(&users_mutex);
                          }
                          printf("GETPREVMSGS_OP COMPLETED!\n");
                    break;

                case GETFILE_OP:
                      if (!registered) {//utente sconosciuto
                          setHeader(&replyHdr, OP_NICK_UNKNOWN, "User is not registered yet");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      // l'utente non è online, operazione non consentita
                      else if (!onlineNick(online_queue, request.hdr.sender)) {
                          setHeader(&replyHdr, OP_FAIL, "User not logged in");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      printf("[%s] requested download of file [%s]\n",request.hdr.sender, request.data.buf);
                      int fp = open(request.data.buf, O_RDONLY);
                      if (fp < 0) {
                          perror("opening file");
                          setHeader(&replyHdr, OP_FAIL, "file not found");
                          sendHeader(cfd, &replyHdr);
                          chattyStats.nerrors++;
                      }
                      else {
                          struct stat file_stats;
                          fstat(fp, &file_stats);
                          char *file = mmap(NULL, file_stats.st_size, PROT_READ, MAP_PRIVATE, fp, 0);
                          replyData.buf = (char *)malloc(file_stats.st_size);
                          memcpy(replyData.buf, file, file_stats.st_size);
                          setData(&replyData, "", file, file_stats.st_size);
                          close(fp);
                          setHeader(&replyHdr, OP_OK, "");
                          sendHeader(cfd, &replyHdr);
                          sendData(cfd, &replyData);
                          chattyStats.nfiledelivered++;
                      }
                    break;

                default:  //operazione non supportata
                    break;
            }
            push(task_queue, cfd);
        }
    }
  quit:
    printf("SERVER STATE: offline\n");
    pthread_exit(0);
}

/*
 ******************************************************
                  Server Main
 ******************************************************
 */
int main(int argc, char *argv[]) {

    if (argc < 3) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    running = 1;

    //creo una maschera per i segnali che mi interessa gestire
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGUSR1);

    //installo il mio handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigHandler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1)
        perror("sigaction SIGINT");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        perror("sigaction SIGTERM");
    if (sigaction(SIGQUIT, &sa, NULL) == -1)
        perror("sigaction SIGQUIT");
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
        perror("sigaction SIGUSR1");

    printf("/********* Server started! *********/\n\n");
    memset(&config, 0, sizeof(config_st));
    serverConfig(argv[2]);

    /* tabella hash nella quale verrano registrati i client */
    usersDB = icl_hash_create(NBUCKETS, &hash_pjw, &string_compare);

    /* coda dei task che verranno svolti dai worker threads */
    task_queue = initQueue();

    /* code degli utenti online */
    online_queue = initQueue();

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    socklen_t addrSize = strlen(config.UnixPath);
    strncpy(addr.sun_path, config.UnixPath, addrSize);

    int ssfd = socket(AF_UNIX, SOCK_STREAM, 0);
    /* binding del file descriptor del socket creato con l'indirizzo specificato nel file di configurazione */
    if (bind(ssfd, (struct sockaddr_un *) &addr, sizeof(addr)) != 0) {
       perror("bind()");
       return -1;
    }

    if (listen(ssfd, config.MaxConnections) != 0) {
        perror("listen()");
        return -1;
    }

    /*  inizializzo un pool di worker threads che gestiranno le richieste dei client */
    pthread_t w_pool[config.ThreadsInPool];
    for (int i=0; i<config.ThreadsInPool; i++) {

        if (pthread_create(&w_pool[i], NULL , wexec, usersDB) < 0) {
            perror("Creating thread_pool");
            return -1;
        }
    }
    /* creo la cartella DirName nella quale verranno salvati i file scaricati dal server */
    if (mkdirat(AT_FDCWD, config.DirName,S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
        if (errno == EEXIST) {
           char ans;
           printf("Directory already exists\n");
           printf("Delete old directory and create a new one? [y/n]: ");
           scanf("%c", &ans);
           if (ans == 'y' || ans == 'Y'){
              if (rmdir(config.DirName) == 0) {
                  printf("Successfully deleted old directory\n");
                  if (mkdirat(AT_FDCWD, config.DirName,S_IRWXU | S_IRWXG | S_IRWXO) != 0) {

                  }
              }
              else perror("deleting old directory");
           }
        }
    }
    else printf("Working directory created...\n");
    /* inizio il ciclo d'acquisizione di connessioni dai client */
    printf("Listening for incoming connections...\n\n");

    long rfd;
    long max_fd = ssfd;
    struct timespec timeout;
    message_hdr_t replyHdr;

    while (running > 0) {
        timeout.tv_sec = 0;
        timeout.tv_nsec = 260;
        memset(&replyHdr, 0, sizeof(message_hdr_t));
        FD_ZERO(&fds);
        FD_SET(ssfd, &fds);

        int sel = pselect(max_fd +1, &fds, NULL, NULL, &timeout, &mask);

        if (sel == 0); //printf("(Timed out)\n");
        if (sel < 0) {
            perror("select()");
            return -1;
        }

        for (rfd = 3; rfd<=max_fd; rfd++) {

            if (FD_ISSET(rfd, &fds)) {
                // se il filedescriptor pronto è quello del server,
                // allora un client vuole aprire una connessione
                if (rfd == ssfd) {
                    //  inizializzo un nuovo filedescriptor per la connessione
                    long new_fd;
                    if ((new_fd = accept(ssfd, (struct sockaddr *)&addr, &addrSize)) == -1) {
                        perror("accept()");
                        break;
                    }
                    /* verifico che ci siano slot liberi per accettare la nuova connessione */
                    pthread_mutex_lock(&conn_mutex);
                    if (activeConns < config.MaxConnections) {
                        pthread_mutex_unlock(&conn_mutex);
                        FD_SET(new_fd, &fds);
                        if (new_fd > max_fd) max_fd = new_fd;
                    }
                    else {  // altrimenti rispondo con un messaggio di errore
                        memset(&replyHdr, 0, sizeof(message_hdr_t));
                        setHeader(&replyHdr, OP_FAIL, "Server");
                        sendHeader(new_fd, &replyHdr);
                    }
                }

                // altrimenti un client già connesso vuole inviare una richiesta
                else {
                    // se non ci sono slot liberi per gestire ulteriori client, rifiuto la richiesta
                    pthread_mutex_lock(&conn_mutex);
                    if (activeConns >= config.MaxConnections) {
                        pthread_mutex_unlock(&conn_mutex);
                        setHeader(&replyHdr, OP_FAIL, "Server");
                        sendHeader(rfd, &replyHdr);
                    }
                    // altrimenti aggiungo il file descriptor in coda
                    else {
                        pthread_mutex_unlock(&conn_mutex);
                        FD_CLR(rfd, &fds);

                        if (push(task_queue,(long) rfd) != 0) {
                            printf("fd_Push error!\n");
                            break;
                        }
                        //segnalo ai thread workers la presenza di una richiesta da gestire
                        pthread_mutex_lock(&conn_mutex);
                        activeConns++;
                        printf("[SERVER]: active connections: %d\n", activeConns);
                        pthread_mutex_unlock(&conn_mutex);
                        pthread_cond_broadcast(&onlineUsers);
                    }
                }
            }
        }
    }
    printf("Shutdown procedure started...\n");
    //attendo la terminazione di tutti i worker threads
    int retval;
    for (int i=0; i<config.ThreadsInPool; i++) {
        pthread_cond_broadcast(&onlineUsers);
        pthread_join(w_pool[i], &retval);
        printf("Thread #%ld joined with exit value %d\n", getpid(w_pool[i]), retval);
    }
    //libero la memoria allocata
    deleteQueue(task_queue);
    deleteQueue(online_queue);
    icl_hash_destroy(usersDB, free, destroyHistory);
    close(ssfd);
    unlink(config.UnixPath);

    printf("Memory cleaned up, shutdown complete\n");

    return 0;
}
