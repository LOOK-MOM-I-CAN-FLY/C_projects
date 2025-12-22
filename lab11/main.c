#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_READERS 10
#define BUFFER_SIZE 128

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"

char shared_array[BUFFER_SIZE] = "Empty";
int record_counter = 0;
int readers_finished = 0;
int data_ready = 0;
int keep_running = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_readers = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_writer = PTHREAD_COND_INITIALIZER;

void* writer_thread(void* arg) {
    (void)arg;
    while (keep_running) {
        pthread_mutex_lock(&mutex);

        while (readers_finished < NUM_READERS && record_counter > 0) {
            pthread_cond_wait(&cond_writer, &mutex);
        }

        record_counter++;
        readers_finished = 0;
        data_ready = 1;
        
        snprintf(shared_array, BUFFER_SIZE, "Record ID: %d", record_counter);
        printf(COLOR_RED "[WRITER] Updated array to: %s" COLOR_RESET "\n", shared_array);

        pthread_cond_broadcast(&cond_readers);
        pthread_mutex_unlock(&mutex);

        usleep(1000000);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    long tid = (long)arg;
    int last_read_id = 0;

    while (keep_running) {
        pthread_mutex_lock(&mutex);

        while (last_read_id == record_counter || !data_ready) {
            pthread_cond_wait(&cond_readers, &mutex);
        }

        printf(COLOR_GREEN "[READER %ld] Current state: %s" COLOR_RESET "\n", tid, shared_array);
        
        last_read_id = record_counter;
        readers_finished++;

        if (readers_finished == NUM_READERS) {
            data_ready = 0;
            pthread_cond_signal(&cond_writer);
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;

    pthread_create(&writer, NULL, writer_thread, NULL);
    for (long i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, reader_thread, (void*)i);
    }

    getchar();
    keep_running = 0;

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond_readers);
    pthread_cond_broadcast(&cond_writer);
    pthread_mutex_unlock(&mutex);

    pthread_cancel(writer);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_cancel(readers[i]);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_readers);
    pthread_cond_destroy(&cond_writer);

    return 0;
}
