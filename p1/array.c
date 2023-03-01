#include <errno.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "options.h"

#define DELAY_SCALE 1000


struct array {
    int size;
    int *values;
    mtx_t **value_mutexes;
};

void initialize_array (struct array *array, int array_size) {

    mtx_t **mutexes;

    array -> size = array_size;
    array -> values  = malloc (array -> size * sizeof (int));
    memset (array -> values, 0, array -> size * sizeof (int));

    mutexes = malloc (sizeof (mtx_t) * (array -> size));

    if (mutexes == NULL) {
        printf ("not enough memory\n");
        exit (1);
    }

    int i = array -> size;
    while (i-- > 0) {

        mutexes [i] = malloc (sizeof (mtx_t));

        if (mutexes [i] == NULL) {
            printf ("not enough memory\n");
            exit (1);
        }
        mtx_init (mutexes [i], mtx_plain);
    }
    array -> value_mutexes = mutexes;
}

struct iterations {
    int total;
    int increment;
    mtx_t *increment_mutex;
    int swap;
    mtx_t *swap_mutex;
};

void initialize_iterations (struct iterations *iterations, int total_iterations) {

    mtx_t *increment_mutex;
    mtx_t *swap_mutex;

    iterations -> total = total_iterations;
    iterations -> increment = 0;
    iterations -> swap = 0;

    increment_mutex = malloc (sizeof (mtx_t));
    swap_mutex = malloc (sizeof (mtx_t));

    if ((increment_mutex == NULL) || (swap_mutex == NULL)) {
        printf ("not enough memory\n");
        exit (1);
    }
    mtx_init (increment_mutex, mtx_plain);
    mtx_init (swap_mutex, mtx_plain);

    iterations -> increment_mutex = increment_mutex;
    iterations -> swap_mutex = swap_mutex;
}

struct thread_arguments {
    int thread_number;
    int delay;
    struct array *array;
    struct iterations *iterations;
};

struct thread_info {
    thrd_t   id;
    struct thread_arguments *arguments;
};

void apply_delay (int delay) {
    int i = delay * DELAY_SCALE;
    while (i-- > 0);
}

int increment (void *pointer) {

    struct thread_arguments *arguments = pointer;
    int delay = arguments -> delay;
    int position, value;

    while (1) {

        mtx_lock (arguments -> iterations -> increment_mutex);
        if (arguments -> iterations -> increment == arguments -> iterations -> total)
            break;

        arguments -> iterations -> increment ++;
        mtx_unlock (arguments -> iterations -> increment_mutex);

        position = rand() % (arguments -> array -> size);

        mtx_lock (arguments -> array -> value_mutexes [position]);

        printf("thread #%d increasing position %d\n",
               arguments -> thread_number , position);

        value = arguments -> array -> values [position];
        apply_delay (delay);

        value ++;
        apply_delay (delay);

        arguments -> array -> values [position] = value;
        apply_delay (delay);

        mtx_unlock (arguments -> array -> value_mutexes [position]);
    }
    return 0;
}

int swap (void *pointer) {

    struct thread_arguments *arguments = pointer;
    int delay = arguments -> delay;
    int fst_position, snd_position, aux_value;

    while (1) {

        mtx_lock (arguments -> iterations -> swap_mutex);
        if (arguments -> iterations -> swap == arguments -> iterations -> total)
            break;

        arguments -> iterations -> swap ++;
        mtx_unlock (arguments -> iterations -> swap_mutex);

        fst_position = rand() % (arguments -> array -> size);
        do
            snd_position = rand() % (arguments -> array -> size);
        while (fst_position == snd_position);

        mtx_lock (arguments -> array -> value_mutexes [fst_position]);
        if (mtx_trylock (arguments -> array -> value_mutexes [snd_position])) {

            mtx_unlock (arguments -> array -> value_mutexes [fst_position]);

            mtx_lock (arguments -> iterations -> swap_mutex);
            arguments -> iterations -> swap --;
            mtx_unlock (arguments -> iterations -> swap_mutex);

            continue;
        }

        aux_value = arguments -> array -> values [fst_position];
        apply_delay (delay);

        printf ("thread #%d swapping values at positions %d & %d\n",
                arguments -> thread_number, fst_position, snd_position);

        arguments -> array -> values [fst_position] = arguments -> array -> values [snd_position];
        apply_delay (delay);

        arguments -> array -> values [snd_position] = aux_value;
        apply_delay (delay);

        mtx_unlock (arguments -> array -> value_mutexes [fst_position]);
        mtx_unlock (arguments -> array -> value_mutexes [snd_position]);
    }
    return 0;
}

struct thread_info *start_threads (struct options options, struct array *array, struct iterations *iterations) {

    struct thread_info *threads;

    printf ("creating %d threads\n", options.num_threads * 2);
    threads = malloc (sizeof (struct thread_info) * options.num_threads * 2);

    if (threads == NULL) {
        printf ("not enough memory");
        exit(1);
    }

    int i = options.num_threads * 2;
    thrd_start_t start_function;
    while (i-- > 0) {

        threads[i].arguments = malloc (sizeof (struct thread_arguments));

        threads[i].arguments -> thread_number = i;
        threads[i].arguments -> delay = options.delay;
        threads[i].arguments -> array = array;
        threads[i].arguments -> iterations = iterations;

        if (i >= options.num_threads)
            start_function = swap;
        else
            start_function = increment;

        if (0 != thrd_create (&threads[i].id, start_function, threads[i].arguments)) {
            printf ("could not create thread #%d", i);
            exit (1);
        }
    }
    return threads;
}

void print_array (struct array array) {
    int total = 0;
    int i = array.size;

    while (i-- > 0) {
        total += array.values[i];
        printf ("%d ", array.values[i]);
    }
    printf ("\ntotal: %d\n", total);
}

void wait (struct options options, struct array *array, struct iterations *iterations, struct thread_info *threads) {
 
    int i = options.num_threads * 2;
    while (i-- > 0)
        thrd_join (threads[i].id, NULL);

    print_array (*array);

    i = options.num_threads * 2;
    while (i-- > 0)
        free (threads[i].arguments);

    i = array -> size;
    while (i-- > 0){
        mtx_destroy (array -> value_mutexes [i]);
        free (array -> value_mutexes [i]);
    }

    mtx_destroy (iterations -> increment_mutex);
    mtx_destroy (iterations -> swap_mutex);
    free (iterations -> increment_mutex);
    free (iterations -> swap_mutex);

    free (array -> value_mutexes);
    free (threads);
    free (array -> values);
}


int main (int argc, char **argv) {

    struct options      options;
    struct array        array;
    struct iterations   iterations;
    struct thread_info  *threads;

    srand (time (NULL));

    options.num_threads  = 5;
    options.array_size   = 10;
    options.iterations   = 100;
    options.delay        = 1000;

    read_options (argc, argv, &options);

    initialize_array (&array, options.array_size);
    initialize_iterations (&iterations, options.iterations);

    threads = start_threads (options, &array, &iterations);
    wait (options, &array, &iterations, threads);

    return 0;
}
