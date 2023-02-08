#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "options.h"

#define DELAY_SCALE 1000


struct array {
    int size;
    int *values;
};

void initialize_array (struct array *array, int array_size) {
    array -> size = array_size;
    array -> values  = malloc (array -> size * sizeof (int));
    memset (array -> values, 0, array -> size * sizeof (int));
}

struct thread_arguments {
    int iterations;         // number of increments
    int thread_number;      // application defined thread
    int delay;              // delay between operations
    struct array *array;    //access to shared array
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

    while ( arguments -> iterations -- ) {

        position = rand() % arguments -> array -> size;

        printf("%d increasing position %d\n", arguments -> thread_number , position);

        value = arguments -> array -> values [position];
        apply_delay (delay);

        value ++;
        apply_delay (delay);

        arguments -> array -> values [position] = value;
        apply_delay (delay);
    }
    return 0;
}

struct thread_info *start_threads (struct options options, struct array *array) {

    struct thread_info *threads;

    printf ("creating %d threads\n", options.num_threads);
    threads = malloc (sizeof (struct thread_info) * options.num_threads);

    if (threads == NULL) {
        printf ("not enough memory");
        exit(1);
    }

    printf ("a");
    int i = options.num_threads;
    while ( i -- > 0 ) {
        
        threads[i].arguments = malloc (sizeof (struct thread_arguments));

        threads[i].arguments -> iterations = options.num_threads;
        threads[i].arguments -> thread_number = i;
        threads[i].arguments -> delay = options.delay;

        if (0 != pthread_create(&threads[i].id, NULL, increment, threads[i].arguments)) {
            printf ("could not create thread #%d", i);
            exit (1);
        }
    }
    return NULL;
}

void close (struct options options, struct array *array, struct thread_info *threads) {
 
    int i = options.num_threads;
    //while ( i -- > 0 )
    for (int i = 0; i < options.num_threads; i++)
        pthread_join (threads[i].id, NULL);

    i = options.num_threads;
    while ( i -- > 0 )
        free (threads[i].arguments);

    free (array -> values);
    free (threads);

}


void print_array (struct array array) {
    int total = 0;
    int i = array.size;

    while ( i -- > 0 ) {
        total += array.values[i];
        printf ("%d ", array.values[i]);
    }
    printf ("\ntotal: %d\n", total);
}


int main (int argc, char **argv) {

    struct options      options;
    struct array        array;
    struct thread_info  *threads;

    srand (time (NULL));

    // Default values for the options
    options.num_threads  = 5;
    options.array_size   = 10;
    options.iterations   = 100;
    options.delay        = 1000;

    read_options (argc, argv, &options);

    initialize_array (&array, options.num_threads);

    threads = start_threads (options, &array);
    close (options, &array, threads);

    print_array (array);

    free (array.values);

    return 0;
}
