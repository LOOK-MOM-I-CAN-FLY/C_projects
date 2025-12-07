#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define NUM_READERS 10
#define NUM_WRITES 20      
#define BUFFER_SIZE 256


char shared_buffer[BUFFER_SIZE];
int record_counter = 0;         
int writer_finished = 0;       


pthread_mutex_t mutex;


void* writer_thread(void* arg) {
    for (int i = 0; i < NUM_WRITES; i++) {

        pthread_mutex_lock(&mutex);
        
        record_counter++;
        snprintf(shared_buffer, BUFFER_SIZE, "Запись #%d", record_counter);
        printf("[Writer] Обновил буфер: %s\n", shared_buffer);
        
        pthread_mutex_unlock(&mutex);

        usleep(100000); // 100ms
    }

    pthread_mutex_lock(&mutex);
    writer_finished = 1;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* reader_thread(void* arg) {
    long reader_id = (long)arg;
    pthread_t sys_tid = pthread_self();

    while (1) {
  
        pthread_mutex_lock(&mutex);
        
        if (writer_finished) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (strlen(shared_buffer) == 0) {
            pthread_mutex_unlock(&mutex);
            usleep(50000); 
            continue;
        }

        printf("Reader #%ld (TID: %lu) прочитал: %s\n", reader_id, (unsigned long)sys_tid, shared_buffer);
        
        pthread_mutex_unlock(&mutex);
    

        usleep(200000); // 200ms
    }
    
    return NULL;
}

int main() {
    pthread_t writer;
    pthread_t readers[NUM_READERS];
    int reader_ids[NUM_READERS];

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Ошибка инициализации мьютекса");
        return 1;
    }

    memset(shared_buffer, 0, BUFFER_SIZE);

    printf("--- Запуск программы ---\n");

    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        perror("Ошибка создания писателя");
        return 1;
    }

    for (int i = 0; i < NUM_READERS; i++) {
        if (pthread_create(&readers[i], NULL, reader_thread, (void*)(long)i) != 0) {
            perror("Ошибка создания читателя");
            return 1;
        }
    }

    pthread_join(writer, NULL);
    printf("--- Писатель завершил работу ---\n");

    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    printf("--- Программа успешно завершена ---\n");

    return 0;
}
