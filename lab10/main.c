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
pthread_rwlock_t rwlock;
pthread_barrier_t barrier_ready;
pthread_barrier_t barrier_done;
int record_counter = 0;
int keep_running = 1;

void* writer_thread(void* arg) {
    while (keep_running) {
        pthread_rwlock_wrlock(&rwlock);
        record_counter++;
        snprintf(shared_array, BUFFER_SIZE, "Record ID: %d", record_counter);
        printf(COLOR_RED "[WRITER] Updated array to: %s" COLOR_RESET "\n", shared_array);
        pthread_rwlock_unlock(&rwlock);

        pthread_barrier_wait(&barrier_ready);
        pthread_barrier_wait(&barrier_done);
        
        usleep(500000);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    long tid = (long)arg;
    while (keep_running) {
        pthread_barrier_wait(&barrier_ready);

        pthread_rwlock_rdlock(&rwlock);
        printf(COLOR_GREEN "[READER %ld] Current state: %s" COLOR_RESET "\n", tid, shared_array);
        pthread_rwlock_unlock(&rwlock);

        pthread_barrier_wait(&barrier_done);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;

    pthread_rwlock_init(&rwlock, NULL);
    pthread_barrier_init(&barrier_ready, NULL, NUM_READERS + 1);
    pthread_barrier_init(&barrier_done, NULL, NUM_READERS + 1);

    pthread_create(&writer, NULL, writer_thread, NULL);
    for (long i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, reader_thread, (void*)i);
    }

    getchar();
    keep_running = 0;

    pthread_cancel(writer);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_cancel(readers[i]);
    }

    pthread_rwlock_destroy(&rwlock);
    pthread_barrier_destroy(&barrier_ready);
    pthread_barrier_destroy(&barrier_done);

    return 0;
}
