/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file config.h
 * @author  Pietro Scarso 544175
 * @brief File contenente alcune define con valori massimi utilizzabili e strutture dati
 *
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH                  32
#define NBUCKETS                       1024

typedef struct _config {
      char *UnixPath;
      char *DirName;
      char *StatFileName;
      int MaxConnections;
      int MaxMsgSize;
      int ThreadsInPool;
      int MaxFileSize;
      int MaxHistMsgs;
} config_st;

// struttura dati utilizzata per rappresentare un utente
typedef struct _user_t {
    long sock;
    char *nick;
} user_t;

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
