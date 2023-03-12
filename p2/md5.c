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

    return (1);
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


void sum (struct options opt) {

    char *ent;
    FILE *out;
    struct file_md5 *md5;
    int dirname_len;

    queue in_q  = q_create (1);
    queue out_q = q_create (opt.queue_size);

    thrd_t id;
    struct scanner_thread_arguments *thread_arguments;
    thread_arguments = malloc (sizeof (struct scanner_thread_arguments));

    thread_arguments -> directory = opt.directory;
    thread_arguments -> queue     = &in_q;

    thrd_create (&id, get_entries, thread_arguments);

    while ((ent = q_remove (in_q)) != NULL) {
        md5 = malloc (sizeof (struct file_md5));

        md5 -> file = ent;
        sum_file (md5);

        q_insert (out_q, md5);
    }

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

    thrd_join (id, NULL);

    free (thread_arguments);
    fclose (out);
    q_destroy (in_q);
    q_destroy (out_q);
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
