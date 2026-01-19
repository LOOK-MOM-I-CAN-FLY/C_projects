#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h> 

#define NUM_READERS 10
#define BUFFER_SIZE 128

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"

char shared_array[BUFFER_SIZE];
int record_counter = 0;
int readers_count = 0;
int update_pending = 0;
volatile sig_atomic_t keep_running = 1;

pthread_mutex_t mutex;
pthread_cond_t cond_readers;
pthread_cond_t cond_writer;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

void* writer_thread(void* arg) {
    (void)arg;
    while (keep_running) {
        pthread_mutex_lock(&mutex);

        while (readers_count < NUM_READERS && record_counter > 0 && keep_running) {
            pthread_cond_wait(&cond_writer, &mutex);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        record_counter++;
        snprintf(shared_array, BUFFER_SIZE, "Record ID: %d", record_counter);
        printf(COLOR_RED "[WRITER] Обновил данные: %s" COLOR_RESET "\n", shared_array);

        readers_count = 0;
        update_pending = 1;

        pthread_cond_broadcast(&cond_readers);
        pthread_mutex_unlock(&mutex);

        usleep(1000000);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    long tid = (long)arg;
    int my_last_read_id = 0;

    while (keep_running) {
        pthread_mutex_lock(&mutex);

        while ((record_counter == my_last_read_id || !update_pending) && keep_running) {
            pthread_cond_wait(&cond_readers, &mutex);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        printf(COLOR_GREEN "[READER %ld] Прочитал: %s" COLOR_RESET "\n", tid, shared_array);
        my_last_read_id = record_counter;
        readers_count++;

        if (readers_count == NUM_READERS) {
            update_pending = 0;
            pthread_cond_signal(&cond_writer);
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;

    signal(SIGINT, handle_sigint);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_readers, NULL);
    pthread_cond_init(&cond_writer, NULL);

    printf("--- Программа запущена. Нажмите Ctrl+C для выхода ---\n");
    fflush(stdout); 

    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) return 1;
    for (long i = 0; i < NUM_READERS; i++) {
        if (pthread_create(&readers[i], NULL, reader_thread, (void*)i) != 0) return 1;
    }

    while (keep_running) {
        pause(); 
    }

    printf("\nЗавершение работы... Ожидаем потоки...\n");
    
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond_writer);
    pthread_cond_broadcast(&cond_readers);
    pthread_mutex_unlock(&mutex);

    pthread_join(writer, NULL);
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_readers);
    pthread_cond_destroy(&cond_writer);

    printf("Все ресурсы очищены.\n");
    return 0;
}
