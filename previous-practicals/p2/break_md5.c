#include <sys/types.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PASS_LEN 6
#define THREADS 4
#define PRO_BAR "#########################"

struct found_data {
    pthread_mutex_t *mutexPass, *mutexIterations;
    int iterations, hash;
    char **md5Pass;
};

struct md5_thread {
    int n, *it;
    struct found_data *data;
};

struct thread_data {
    pthread_t thread;
    struct md5_thread *args;
};

long ipow(long base, int exp) {
    long res = 1;
    for (;;) {
        if (exp & 1) res *= base;
        exp >>= 1;
        if (!exp) break;
        base *= base;
    }
    return res;
}

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i = PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *break_pass(void *ptr) {
    struct md5_thread *md5Pass = ptr;
    char *aux;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char res2[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                               // lowercase chars => 26 ^ PASS_LEN  different cases
    for(long i = md5Pass->n; i < bound; i += THREADS) {
        if (md5Pass->data->hash) {
            long_to_pass(i, pass);
            MD5(pass, PASS_LEN, res);
            md5Pass->data->iterations++;
            *md5Pass->it += 1;
            for (int j = 0; j < md5Pass->data->hash; j++) {
                hex_to_num(md5Pass->data->md5Pass[j], res2);

                if (!memcmp(res, res2, MD5_DIGEST_LENGTH)) {
                    pthread_mutex_lock(md5Pass->data->mutexPass);
                    printf("%s: %s\n", md5Pass->data->md5Pass[j], pass);

                    for (int k = j; k < md5Pass->data->hash-1; k++) {
                        aux = md5Pass->data->md5Pass[k+1];
                        md5Pass->data->md5Pass[k] = md5Pass->data->md5Pass[k+1];
                        md5Pass->data->md5Pass[k] = aux;
                    }
                    md5Pass->data->hash--;
                    pthread_mutex_unlock(md5Pass->data->mutexPass);
                    break; // Found it!
                }
            }
        } else break; // All the passwords have already been found
    }
    free(pass);
    return NULL;
}

void init_data(struct found_data *data, char *md5Pass[], int Nhash) {
    if ((data->mutexPass        = malloc(sizeof(pthread_mutex_t))) == NULL
     || (data->mutexIterations  = malloc(sizeof(pthread_mutex_t))) == NULL
     || (data->md5Pass          = malloc(sizeof(char*) * Nhash))   == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    pthread_mutex_init(data->mutexPass, NULL);
    pthread_mutex_init(data->mutexIterations, NULL);

    data->hash = Nhash;
    data->iterations = 0;
    for (int i = 0; i < Nhash; i++) data->md5Pass[i] = md5Pass[i+1];
}

void *pro_bar(void *ptr) {
    struct md5_thread *thread = ptr;
    int found, C, barC = 25, passChecked;
    double iterations, percentage;
    long bound = ipow(26, PASS_LEN);

    for(;;) {
        pthread_mutex_lock(thread->data->mutexPass);
        found = thread->data->hash;
        pthread_mutex_unlock(thread->data->mutexPass);
        if (found == 0) break;

        passChecked = *thread->it;
        sleep(1);

        pthread_mutex_lock(thread->data->mutexIterations);
        iterations = (double) thread->data->iterations;
        pthread_mutex_unlock(thread->data->mutexIterations);

        percentage = 100 * (iterations/(double)bound);
        C = (int) (percentage * (1./(100./barC)));

        printf("\rPasswords per second = %8.0d || %.2f%% [%.*s%*s] ",
               *thread->it - passChecked, percentage, C, PRO_BAR, barC - C, "");
        fflush(stdout);
    }

    printf("\rAll passwords have been found successfully\n");
    return NULL;
}

struct thread_data *start_threads(struct found_data *data) {
    struct thread_data *threads;
    int i = 0, it = 0, *totalIterations = &it;

    if ((threads = malloc(sizeof(struct thread_data) * THREADS)) == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    do {
        threads[i].args = malloc(sizeof(struct md5_thread));
        threads[i].args->n = i;
        threads[i].args->data = data;
        threads[i].args->it = totalIterations;

        if (i < THREADS) {
            if (pthread_create(&threads[i].thread, NULL, break_pass, threads[i].args)) {
                printf("Thread #%d of %d was not created successfully", i, THREADS+1);
                exit(1);
            }
        } else {
            if (pthread_create(&threads[i].thread, NULL, pro_bar, threads[i].args)) {
                printf("Thread #%d of %d was not created successfully", i, THREADS+1);
                exit(1);
            }
        }
        i++;
    } while (i < THREADS + 1);

    return threads;
}

void wait(struct thread_data *threads, struct found_data *data) {
    for (int i = 0; i < THREADS + 1; i++) pthread_join(threads[i].thread, NULL);
    for (int i = 1; i < THREADS; i++) free(threads[i].args);

    pthread_mutex_destroy(data->mutexPass);
    free(data->mutexPass);
    pthread_mutex_destroy(data->mutexIterations);
    free(data->mutexIterations);

    free(data->md5Pass);
    free(threads);
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    struct thread_data *threads;
    struct found_data data;
    unsigned char md5_num[MD5_DIGEST_LENGTH];

    hex_to_num(argv[1], md5_num);
    init_data(&data, argv, argc-1);
    threads = start_threads(&data);
    wait(threads, &data);

    return 0;
}
