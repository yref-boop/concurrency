#include <errno.h>
#include <pthread.h>
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
    pthread_mutex_t **value_mutexes;
    int increment_iterations;
    pthread_mutex_t *increment_mutex;
    int swap_iterations;
    pthread_mutex_t *swap_mutex;
};

void initialize_array (struct array *array, int array_size) {

    pthread_mutex_t **value_mutexes;
    pthread_mutex_t *iterations_mutex;
    pthread_mutex_t *swap_mutex;

    array -> size = array_size;
    array -> increment_iterations = 0;
    array -> swap_iterations = 0;
    array -> values  = malloc (array -> size * sizeof (int));
    memset (array -> values, 0, array -> size * sizeof (int));

    iterations_mutex = malloc (sizeof (pthread_mutex_t));
    swap_mutex = malloc (sizeof (pthread_mutex_t));
    value_mutexes = malloc (sizeof(pthread_mutex_t) * (array -> size));

    if ((iterations_mutex == NULL) || (swap_mutex == NULL) || (value_mutexes == NULL)) {
        printf ("not enough memory\n");
        exit (1);
    }
    pthread_mutex_init (iterations_mutex, NULL);
    array -> increment_mutex = iterations_mutex;

    pthread_mutex_init (swap_mutex, NULL);
    array -> swap_mutex = swap_mutex;

    int i = array -> size;
    while ( i -- >  0 ) {

        value_mutexes [i] = malloc (sizeof (pthread_mutex_t));

        if (value_mutexes [i] == NULL) {
            printf ("not enough memory\n");
            exit (1);
        }
        pthread_mutex_init (value_mutexes [i], NULL);
	}
    array -> value_mutexes = value_mutexes;
}

struct thread_arguments {
    int iterations;         // number of increments
    int thread_number;      // application defined thread
    int delay;              // delay between operations
    struct array *array;    // access to shared array
};

struct thread_info {
    pthread_t   id;
    struct thread_arguments *arguments;
};

void apply_delay (int delay) {

    int i = delay * DELAY_SCALE;
    while ( i -- > 0 );
}

void *increment (void *pointer) {

    struct thread_arguments *arguments = pointer;
    int delay = arguments -> delay;
    int position, value;

    while ( 1 ) {

        position = rand() % (arguments -> array -> size);

        if ( arguments -> array -> increment_iterations == arguments -> iterations ) break;
        else {


            pthread_mutex_lock (arguments -> array -> increment_mutex);
            arguments -> array -> increment_iterations ++;
            pthread_mutex_unlock (arguments -> array -> increment_mutex);

            pthread_mutex_lock (arguments -> array -> value_mutexes [position]);

            printf("thread #%d increasing position %d\n",
                   arguments -> thread_number , position);

            value = arguments -> array -> values [position];
            apply_delay (delay);

            value ++;
            apply_delay (delay);

            arguments -> array -> values [position] = value;
            apply_delay (delay);

            pthread_mutex_unlock (arguments -> array -> value_mutexes [position]);
        }
    }
    return NULL;
}

void *swap (void *pointer) {

    struct thread_arguments *arguments = pointer;
    int delay = arguments -> delay;
    int position1, position2, aux_value;

    while ( 1 ) {

        position1 = rand() % (arguments -> array -> size);
        do
            position2 = rand() % (arguments -> array -> size);
        while (position1 == position2);

        if ( arguments -> array -> swap_iterations == arguments -> iterations ) break;
        else {

            pthread_mutex_lock (arguments -> array -> swap_mutex);
            arguments -> array -> swap_iterations ++;
            pthread_mutex_unlock (arguments -> array -> swap_mutex);

            pthread_mutex_lock (arguments -> array -> value_mutexes [position1]);
            if (pthread_mutex_trylock (arguments -> array -> value_mutexes [position2])) {
                pthread_mutex_unlock (arguments -> array -> value_mutexes [position1]);
                continue;
            }

            aux_value = arguments -> array -> values [position1];
            apply_delay (delay);

            printf ( "thread #%d swapping values at position %d & %d\n", arguments -> thread_number, position1, position2);

            arguments -> array -> values [position1] = arguments -> array -> values [position2];
            apply_delay (delay);

            arguments -> array -> values [position2] = aux_value;
            apply_delay (delay);

            pthread_mutex_unlock (arguments -> array -> value_mutexes [position1]);
            pthread_mutex_unlock (arguments -> array -> value_mutexes [position2]);
        }
    }
    return NULL;
}

struct thread_info *start_threads (struct options options, struct array *array) {

    struct thread_info *threads;

    printf ("creating %d threads\n", options.num_threads * 2);
    threads = malloc (sizeof (struct thread_info) * options.num_threads * 2);

    if (threads == NULL) {
        printf ("not enough memory");
        exit(1);
    }

    array -> increment_iterations = 0;
    array -> swap_iterations = 0;

    int i = options.num_threads * 2;
    while ( i -- > 0 ) {

        threads[i].arguments = malloc (sizeof (struct thread_arguments));

        threads[i].arguments -> iterations = options.iterations;
        threads[i].arguments -> thread_number = i;
        threads[i].arguments -> delay = options.delay;
        threads[i].arguments -> array = array;

        void* start_function;
        if ( i < options.num_threads )
            start_function = increment;
        else
            start_function = swap;

        if (0 != pthread_create (&threads[i].id, NULL, start_function, threads[i].arguments)) {
            printf ("could not create thread #%d", i);
            exit (1);
        }
    }
    return threads;
}

void print_array (struct array array) {
    int total = 0;
    int i = array.size;

    while ( i -- > 0 ) {
        total += array.values[i];
        printf ("%d ", array.values[i]);
    }
    printf ("\ntotal: %d\n", total);

    printf ("increment iterations count: %d\n", array.increment_iterations);
    printf ("swap iterations count: %d\n", array.swap_iterations);
}

void wait (struct options options, struct array *array, struct thread_info *threads) {
 
    int i = options.num_threads * 2;
    while ( i -- > 0 )
        pthread_join (threads[i].id, NULL);

    print_array (*array);

    i = options.num_threads * 2;
    while ( i -- > 0 )
        free (threads[i].arguments);

    i = array -> size;
    while ( i -- > 0 ) {
		pthread_mutex_destroy (array -> value_mutexes [i]);
		free (array -> value_mutexes [i]);
    }
    free (array -> increment_mutex);
    free (array -> swap_mutex);
    free (array -> value_mutexes);
    free (threads);
    free (array -> values);
}


int main (int argc, char **argv) {

    struct options      options;
    struct array        array;
    struct thread_info  *threads;

    srand (time (NULL));

    options.num_threads  = 5;
    options.array_size   = 10;
    options.iterations   = 100;
    options.delay        = 1000;

    read_options (argc, argv, &options);

    initialize_array (&array, options.array_size);

    threads = start_threads (options, &array);
    wait (options, &array, threads);

    return 0;
}
