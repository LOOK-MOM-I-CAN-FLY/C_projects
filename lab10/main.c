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

int record_counter = 0;
volatile int keep_running = 1; 

void* writer_thread(void* arg) {
    (void)arg;
    while (keep_running) {
        pthread_rwlock_wrlock(&rwlock);
        
        record_counter++;
        snprintf(shared_array, BUFFER_SIZE, "Record ID: %d", record_counter);
        printf(COLOR_RED "[WRITER] Обновил массив: %s" COLOR_RESET "\n", shared_array);
        
        pthread_rwlock_unlock(&rwlock);

        usleep(500000); 
    }
    return NULL;
}

void* reader_thread(void* arg) {
    long tid = (long)arg;
    while (keep_running) {
        pthread_rwlock_rdlock(&rwlock);
        
        if (keep_running) { 
            printf(COLOR_GREEN "[READER %ld] Прочитал: %s" COLOR_RESET "\n", tid, shared_array);
        }

        pthread_rwlock_unlock(&rwlock);

        usleep(200000);
    }
    return NULL;
}

int main() {
    pthread_t readers[NUM_READERS];
    pthread_t writer;

    if (pthread_rwlock_init(&rwlock, NULL) != 0) {
        perror("rwlock_init");
        return 1;
    }

    printf("--- Запуск потоков. Нажмите ENTER для остановки ---\n");

    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        perror("create writer");
        return 1;
    }

    for (long i = 0; i < NUM_READERS; i++) {
        if (pthread_create(&readers[i], NULL, reader_thread, (void*)i) != 0) {
            perror("create reader");
            return 1;
        }
    }

    getchar();
    
    printf("Завершение работы... Ожидание потоков...\n");
    keep_running = 0; 

    pthread_join(writer, NULL);

    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    
    printf("Все потоки завершены корректно.\n");

    pthread_rwlock_destroy(&rwlock);

    return 0;
}
