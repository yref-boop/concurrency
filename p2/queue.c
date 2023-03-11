#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <threads.h>

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    mtx_t *mutex;
    cnd_t *empty;
    cnd_t *full;
} _queue;

#include "queue.h"

queue q_create (int size) {

    queue q = malloc (sizeof (_queue));

    q -> size  = size;
    q -> used  = 0;
    q -> first = 0;
    q -> data  = malloc (size * sizeof (void *));

    q -> mutex = malloc (sizeof (mtx_t));
    mtx_init (q -> mutex, mtx_plain);

    q -> empty = malloc (sizeof (cnd_t));
    q -> full  = malloc (sizeof (cnd_t));
    cnd_init (q -> empty);
    cnd_init (q -> full);

    return q;
}

int q_elements (queue q) {
    return q -> used;
}
 
int q_insert (queue q, void *elem) {

    mtx_lock (q -> mutex);
    while (q -> size == q -> used)
        cnd_wait (q -> full, q -> mutex);

    q -> data [(q -> first + q -> used) % q -> size] = elem;
    q -> used++;

    if (q -> used > 0)
        cnd_broadcast (q -> empty);

    mtx_unlock (q -> mutex);
    return 0;
}

void *q_remove (queue q) {

    struct timespec timespec;
    struct timeval timeval;

    // current time +1 sec for timed wait
    if (gettimeofday (&timeval, NULL) != 0)
        printf ("could not set waiting time");

    timespec.tv_sec = timeval.tv_sec;
    timespec.tv_nsec = timeval.tv_usec * 1000;
    timespec.tv_sec += 1;

    mtx_lock (q -> mutex);
    void *res;
    while (q -> used == 0) {
        cnd_timedwait (q -> empty, q -> mutex, &timespec);
        if (q -> used != 0)
            break;
        return NULL;
    }

    res = q -> data [q -> first];

    q -> first = (q -> first + 1) % q -> size;
    q -> used--;

    if (q -> used < q -> size)
        cnd_broadcast (q -> full);

    mtx_unlock (q -> mutex);
    return res;
}

void q_destroy (queue q) {

    mtx_destroy (q -> mutex);
    free (q -> mutex);

    cnd_destroy (q -> empty);
    cnd_destroy (q -> full);
    free (q -> empty);
    free (q -> full);

    free (q -> data);
    free (q);
}
