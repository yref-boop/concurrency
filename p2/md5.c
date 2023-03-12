#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>
#include <threads.h>

#include "options.h"
#include "queue.h"


#define MAX_PATH 1024
#define BLOCK_SIZE (10*1024*1024)
#define MAX_LINE_LENGTH (MAX_PATH * 2)


struct file_md5 {
    char *file;
    unsigned char *hash;
    unsigned int hash_size;
};

 
struct scanner_thread_arguments {
    char *directory;
    queue *queue;
};

struct scanner_thread_info {
    thrd_t id;
    struct scanner_thread_arguments *arguments;
};


struct hash_thread_arguments {
    queue *out_queue;
    queue *in_queue;
    mtx_t *mutex;
};

struct hash_thread_info {
    thrd_t id;
    struct hash_thread_arguments *arguments;
};


int get_entries (void *pointer);


void print_hash (struct file_md5 *md5) {
    for (int i = 0; i < md5 -> hash_size; i++) {
        printf ("%02hhx", md5 -> hash[i]);
    }
}


void read_hash_file (char *file, char *dir, queue q) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *file_name, *hash;
    int hash_len;

    if ((fp = fopen (file, "r")) == NULL) {
        printf ("Could not open %s : %s\n", file, strerror (errno));
        exit (0);
    }

    while (fgets (line, MAX_LINE_LENGTH, fp) != NULL) {
        struct file_md5 *md5 = malloc (sizeof (struct file_md5));
        file_name = strtok (line, ": ");
        hash      = strtok (NULL, ": ");
        hash_len  = strlen (hash);

        md5 -> file      = malloc (strlen (file_name) + strlen (dir) + 2);
        sprintf (md5 -> file, "%s/%s", dir, file_name);
        md5 -> hash      = malloc (hash_len / 2);
        md5 -> hash_size = hash_len / 2;

        for (int i = 0; i < hash_len; i += 2)
            sscanf (hash + i, "%02hhx", &md5 -> hash[i / 2]);

        q_insert (q, md5);
    }
    fclose (fp);
}


void sum_file (struct file_md5 *md5) {
    EVP_MD_CTX *mdctx;
    int nbytes;
    FILE *fp;
    char *buf;

    if ((fp = fopen(md5->file, "r")) == NULL) {
        printf ("could not open %s\n", md5 -> file);
        return;
    }

    buf = malloc (BLOCK_SIZE);
    const EVP_MD *md = EVP_get_digestbyname ("md5");

    mdctx = EVP_MD_CTX_create ();
    EVP_DigestInit_ex (mdctx, md, NULL);

    while ((nbytes = fread (buf, 1, BLOCK_SIZE, fp)) > 0)
        EVP_DigestUpdate (mdctx, buf, nbytes);

    md5 -> hash = malloc (EVP_MAX_MD_SIZE);
    EVP_DigestFinal_ex (mdctx, md5 -> hash, &md5 -> hash_size);

    EVP_MD_CTX_destroy (mdctx);
    free (buf);
    fclose (fp);
}


void recurse (void *pointer) {

    struct scanner_thread_arguments *arguments = pointer;
    struct stat st;

    stat (arguments -> directory, &st);

    if (S_ISDIR (st.st_mode))
        get_entries (arguments);
}


void add_files (void *pointer) {

    struct scanner_thread_arguments *arguments = pointer;
    struct stat st;

    stat (arguments -> directory, &st);

    if (S_ISREG (st.st_mode))
        q_insert (*arguments -> queue, strdup (arguments -> directory));
}


void walk_dir (void (*action)(void *arg), void *pointer) {
    struct scanner_thread_arguments *arguments = pointer;
    DIR *d;
    struct dirent *ent;
    char full_path[MAX_PATH];

    queue queue = *arguments -> queue;

    if ((d = opendir (arguments -> directory)) == NULL) {
        printf ("could not open dir %s\n", arguments -> directory);
        return;
    }

    char *directory = arguments -> directory;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp (ent -> d_name, ".") == 0 || strcmp (ent -> d_name, "..") == 0)
            continue;

        snprintf (full_path, MAX_PATH, "%s/%s", directory, ent -> d_name);
        arguments -> directory = full_path;
        action (arguments);
    }
    closedir (d);
}


int get_entries (void *pointer) {

    struct scanner_thread_arguments *arguments = pointer;

    char *directory = arguments -> directory;
    walk_dir (add_files, arguments);

    arguments -> directory = directory;
    walk_dir (recurse, arguments);

    return 0;
}


int get_hash (void *pointer) {

    struct hash_thread_arguments *arguments = pointer;

    struct file_md5 *md5;
    char *entry;

    while (1) {

        mtx_lock (arguments -> mutex);

        if ((entry = q_remove (*arguments -> in_queue)) == NULL)
            break;

        md5 = malloc (sizeof (struct file_md5));
        md5 -> file = entry;
        sum_file (md5);

        q_insert (*arguments -> out_queue, md5);

        mtx_unlock (arguments -> mutex);
    }
    return 0;
}


void check (struct options options) {

    queue in_q;
    struct file_md5 *md5_in, md5_file;

    in_q = q_create (options.queue_size);

    read_hash_file (options.file, options.directory, in_q);

    while ((md5_in = q_remove (in_q))) {
        md5_file.file = md5_in -> file;

        sum_file (&md5_file);

        if (memcmp (md5_file.hash, md5_in -> hash, md5_file.hash_size) != 0) {
            printf ("File %s doesn't match.\nFound:    ", md5_file.file);
            print_hash (&md5_file);
            printf ("\nExpected: ");
            print_hash (md5_in);
            printf ("\n");
        }
        free (md5_file.hash);
        free (md5_in -> file);
        free (md5_in -> hash);
        free (md5_in);
    }
    q_destroy (in_q);
}


void wait (struct scanner_thread_info *scanner_thread, struct hash_thread_info *hash_threads, struct options options) {
 
    thrd_join (scanner_thread -> id, NULL);
    free (scanner_thread);

    mtx_destroy (hash_threads[0].arguments -> mutex);
    free (hash_threads[0].arguments -> mutex);

    int count = options.num_threads;
    while (count -- > 0)
        free (hash_threads[count].arguments);
    free (hash_threads);
}


struct scanner_thread_info *start_scanner_thread (struct options options, queue *queue) {

    thrd_t id;
    struct scanner_thread_info *scanner_thread;
    scanner_thread = malloc (sizeof (struct scanner_thread_info));

    struct scanner_thread_arguments *thread_arguments;
    thread_arguments = malloc (sizeof (struct scanner_thread_arguments));

    thread_arguments -> directory = options.directory;
    thread_arguments -> queue     = queue;

    if (0 != thrd_create (&scanner_thread -> id, get_entries, thread_arguments)) {
        printf ("could not create scanner thread\n");
        exit(1);
    }
    return scanner_thread;
}


struct hash_thread_info *start_hash_threads (struct options options, queue *in_queue, queue *out_queue) {

    struct hash_thread_info *hash_threads;
    int count = options.num_threads;

    hash_threads = malloc (sizeof (struct hash_thread_info) * count);

    if (hash_threads == NULL) {
        printf ("not enough memory for hash thread creation\n");
        exit (1);
    }

    mtx_t *mutex = malloc (sizeof (mtx_t));
    mtx_init (mutex, mtx_plain);

    while (count -- > 0) {
        hash_threads[count].arguments = malloc (sizeof (struct hash_thread_arguments));

        hash_threads[count].arguments -> out_queue = out_queue;
        hash_threads[count].arguments -> in_queue = in_queue;
        hash_threads[count].arguments -> mutex = mutex;

        if (0 != thrd_create (&hash_threads[count].id, get_hash, hash_threads[count].arguments)) {
            printf ("could not create hash thread #%d", count);
            exit (1);
        }
    }
    return hash_threads;
}


void sum (struct options opt) {

    char *ent;
    FILE *out;
    int dirname_len;
    struct file_md5 *md5;

    queue in_q  = q_create (1);
    queue out_q = q_create (opt.queue_size);

    struct scanner_thread_info *scanner_thread;
    scanner_thread = start_scanner_thread (opt, &in_q);

    struct hash_thread_info *hash_threads;
    hash_threads = start_hash_threads (opt, &in_q, &out_q);

    if ((out = fopen (opt.file, "w")) == NULL) {
        printf ("Could not open output file\n");
        exit (0);
    }

    dirname_len = strlen (opt.directory) + 1; // length of dir + /

    while ((md5 = q_remove (out_q)) != NULL) {
        fprintf (out, "%s: ", md5 -> file + dirname_len);

        for (int i = 0; i < md5 -> hash_size; i++)
            fprintf (out, "%02hhx", md5 -> hash[i]);
        fprintf (out, "\n");

        free (md5 -> file);
        free (md5 -> hash);
        free (md5);
    }

    fclose (out);
    q_destroy (in_q);
    q_destroy (out_q);

    wait (scanner_thread, hash_threads, opt);
}


int main (int argc, char *argv[]) {

    struct options opt;

    opt.num_threads = 5;
    opt.queue_size  = 1000;
    opt.check       = true;
    opt.directory   = NULL;
    opt.file        = NULL;

    read_options (argc, argv, &opt);

    if (opt.check)
        check (opt);
    else
        sum (opt);

}
