#include <stdlib.h>
#include <threads.h>

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    mtx_t *mutex;
} _queue;

#include "queue.h"

queue q_create (int size) {

    queue q = malloc (sizeof (_queue));

    q -> size  = size;
    q -> used  = 0;
    q -> first = 0;
    q -> data  = malloc (size * sizeof (void *));

    mtx_t *mutex = malloc (sizeof (mtx_t));
    q -> mutex = mutex;

    mtx_init (q -> mutex, mtx_plain);

    return q;
}

int q_elements (queue q) {
    return q -> used;
}
 
int q_insert (queue q, void *elem) {

    mtx_lock (q -> mutex);

    if (q -> size == q -> used) return -1;

    q -> data [(q -> first + q -> used) % q -> size] = elem;
    q -> used++;

    mtx_unlock (q -> mutex);

    return 0;
}

void *q_remove (queue q) {

    mtx_lock (q -> mutex);

    void *res;
    if (q -> used == 0) return NULL;

    res = q -> data [q -> first];

    q -> first = (q -> first + 1) % q -> size;
    q -> used--;

    mtx_unlock (q -> mutex);
    return res;
}

void q_destroy (queue q) {
    mtx_destroy (q -> mutex);
    free (q -> mutex);
    free (q -> data);
    free (q);
}
