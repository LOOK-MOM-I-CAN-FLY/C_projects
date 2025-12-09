#define _DEFAULT_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h> 


#define NUM_READERS 10
#define BUFFER_SIZE 256
#define DELAY_MICROSECONDS 1500000 


char shared_buffer[BUFFER_SIZE];
int record_counter = 0;
pthread_mutex_t mutex; 


void* writer_thread(void* arg) {

    (void)arg; 

    while (1) {

        usleep(DELAY_MICROSECONDS);

      
        pthread_mutex_lock(&mutex);

        record_counter++;
        snprintf(shared_buffer, BUFFER_SIZE, "Запись #%d", record_counter);
        
        printf("\033[0;31m[WRITER]\033[0m Обновил буфер: %s\n", shared_buffer);

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* reader_thread(void* arg) {
    long my_id = (long)(intptr_t)arg;
    pthread_t sys_tid = pthread_self();

    while (1) {
        usleep(DELAY_MICROSECONDS);

        pthread_mutex_lock(&mutex);

        if (record_counter == 0) {
            printf("\033[0;32m[READER %ld]\033[0m (tid: %lu) Буфер пуст\n", my_id, (unsigned long)sys_tid);
        } else {
            printf("\033[0;32m[READER %02ld]\033[0m (tid: %lu) Прочитал: %s\n", my_id, (unsigned long)sys_tid, shared_buffer);
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t writer;
    pthread_t readers[NUM_READERS];
    int i;

    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Ошибка инициализации мьютекса");
        return 1;
    }

    strcpy(shared_buffer, "Нет данных");

    printf("=== Запуск программы (1 писатель, %d читателей) ===\n", NUM_READERS);
    printf("=== Интервал обновлений: 1.5 секунды ===\n\n");

    if (pthread_create(&writer, NULL, writer_thread, NULL) != 0) {
        perror("Ошибка создания писателя");
        return 1;
    }

    for (i = 0; i < NUM_READERS; i++) {
        if (pthread_create(&readers[i], NULL, reader_thread, (void*)(intptr_t)(i + 1)) != 0) {
            perror("Ошибка создания читателя");
            return 1;
        }
    }

    pthread_join(writer, NULL);
    for (i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}
