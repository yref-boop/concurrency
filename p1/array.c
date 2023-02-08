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

int increment (int id, int iterations, int delay, struct array *array) {

    int position, value;
    int i = iterations;

    while ( i -- > 0 ) {

        position = rand() % array -> size;

        printf("%d increasing position %d\n", id, position);

        value = array -> values [position];
        apply_delay (delay);

        value ++;
        apply_delay (delay);

        array -> values [position] = value;
        apply_delay (delay);
    }

    return 0;
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
    struct options  options;
    struct array    array;

    srand (time (NULL));

    // Default values for the options
    options.num_threads  = 5;
    options.array_size   = 10;
    options.iterations   = 100;
    options.delay        = 1000;

    read_options (argc, argv, &options);

    array.size = options.array_size;
    array.values  = malloc (array.size * sizeof (int));

    memset (array.values, 0, array.size * sizeof (int));

    increment (0, options.iterations, options.delay, &array);

    print_array (array);

    free (array.values);

    return 0;
}
