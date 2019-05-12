#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <queue.h>
#include "config.h"

/**
 * @file queue.c
 * @brief File di implementazione dell'interfaccia per la coda
 */



static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  qcond = PTHREAD_COND_INITIALIZER;

/* ------------------- funzioni di utilita' -------------------- */

static Node_t *allocNode()         { return malloc(sizeof(Node_t));  }
static Queue_t *allocQueue()       { return malloc(sizeof(Queue_t)); }
static void freeNode(Node_t *node) { free((void*)node); }
static void LockQueue()            { pthread_mutex_lock(&qlock);   }
static void UnlockQueue()          { pthread_mutex_unlock(&qlock); }
static void UnlockQueueAndWait()   { pthread_cond_wait(&qcond, &qlock); }
static void UnlockQueueAndSignal() {
    pthread_cond_signal(&qcond);
    pthread_mutex_unlock(&qlock);
}

/* ------------------- interfaccia della coda ------------------ */

Queue_t *initQueue() {
    Queue_t *q = allocQueue();
    if (!q) return NULL;
    q->head = allocNode();
    if (!q->head) return NULL;
    q->head->data = NULL;
    q->head->next = NULL;
    q->tail = q->head;
    q->qlen = 0;
    return q;
}

void deleteQueue(Queue_t *q) {
    while(q->head != q->tail) {
	Node_t *p = (Node_t*)q->head;
	q->head = q->head->next;
	freeNode(p);
    }
    if (q->head) freeNode((void*)q->head);
    free(q);
}

int onlineFd(Queue_t *q, long data) {
    if (q == NULL) {
        return -1;
    }

    LockQueue();
    Node_t *ptr = q->head->next;
    //int found = -1;
    while (ptr != NULL) {
        user_t *usr = (user_t *)ptr->data;
        if (usr->sock == data) {
            UnlockQueue();
            return 1;
        }
        ptr = ptr->next;
    }
    UnlockQueue();

    return (-1);
}

int onlineNick(Queue_t *q, char *data) {
    if (q == NULL) {
        return -1;
    }
    LockQueue();
    Node_t *ptr = q->head->next;
    int found = -1;
    while (ptr != NULL && found == -1) {
        user_t *usr = (user_t *)ptr->data;
        if (strcmp(usr->nick, data) == 0){
            found = 1;
        }
        ptr = ptr->next;
    }
    UnlockQueue();
    return found;
}

long getFd(Queue_t *q, char *data) {
    if (q == NULL) return -1;

    long fd;
    LockQueue();
    Node_t *ptr = q->head->next;
    int found = -1;
    while (ptr != NULL && found == -1) {
        user_t *usr = (user_t *)ptr->data;
        if (strcmp(usr->nick, data) == 0) {
            found = 1;
            fd = (long) usr->sock;
        }
        ptr = ptr->next;
    }
    UnlockQueue();

    return fd;
}

int deleteNode(Queue_t *q, long data) {
    if (q == NULL) {
        return -1;
    }
    LockQueue();
    Node_t *curr = (Node_t *)q->head;
    Node_t *prev = NULL;
    int found = -1;

    while (curr != NULL && found == -1) {
        user_t *usr = (user_t *)curr->next->data;
        if (usr->sock == data) {
            found = 1;
            if (prev == NULL) {
                q->head = q->head->next;
                freeNode(curr);
            }
            else {
                prev->next = curr->next;
                freeNode(curr);
            }
            q->qlen -= 1;
            UnlockQueue();
            return found;
        }
        prev = curr;
        curr = curr->next;
    }
    UnlockQueue();
    return found;
}

int push(Queue_t *q, void *data) {
    Node_t *n = allocNode();
    n->data = data;
    n->next = NULL;
    LockQueue();
    q->tail->next = n;
    q->tail       = n;
    q->qlen      += 1;
    UnlockQueueAndSignal();
    return 0;
}

void *pop(Queue_t *q) {
    LockQueue();
    while(q->head == q->tail) {
	UnlockQueueAndWait();
    }
    // locked
    assert(q->head->next);
    Node_t *n  = (Node_t *)q->head;
    void *data = (q->head->next)->data;
    q->head    = q->head->next;
    q->qlen   -= 1;
    assert(q->qlen>=0);
    UnlockQueue();
    freeNode(n);
    return data;
}

int dumpList(Queue_t *q, char *buf) {
    if (!q) return -1;
    LockQueue();
    memset(buf, '\0', sizeof(MAX_NAME_LENGTH+1) * q->qlen);
    Node_t *curr = q->head->next;
    user_t *usr;
    size_t offset = MAX_NAME_LENGTH+1;
    int i=0;
    while (curr != NULL) {
        usr = (user_t *)curr->data;
        strncpy(&buf[offset * i], usr->nick, (MAX_NAME_LENGTH+1)*sizeof(char));
        i++;
        curr = curr->next;
    }
    UnlockQueue();
    return 1;
}

unsigned long length(Queue_t *q) {
    unsigned long len = q->qlen;
    return len;
}
