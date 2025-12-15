#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#define BUFFER_SIZE 256

char shared_buffer[BUFFER_SIZE];
sem_t semaphore;
int counter = 0;

void* writer_thread(void* args) {
    (void)args;

    while (1) {
        sem_wait(&semaphore);

        counter++;
        snprintf(shared_buffer, BUFFER_SIZE, "Запись №%d", counter);
        printf("[Writer] Записал: %s\n", shared_buffer);

        sem_post(&semaphore);

        sleep(1);
    }
    return NULL;
}

void* reader_thread(void* args) {
    (void)args;

    while (1) {
        sem_wait(&semaphore);

        pthread_t tid = pthread_self();
        printf("[Reader TID: %lu] Прочитал: %s\n", (unsigned long)tid, shared_buffer);

        sem_post(&semaphore);

        usleep(500000); 
    }
    return NULL;
}

int main() {
    pthread_t t_writer, t_reader;

    if (sem_init(&semaphore, 0, 1) != 0) {
        perror("Ошибка инициализации семафора");
        return 1;
    }

    if (pthread_create(&t_writer, NULL, writer_thread, NULL) != 0) {
        perror("Ошибка создания писателя");
        return 1;
    }
    if (pthread_create(&t_reader, NULL, reader_thread, NULL) != 0) {
        perror("Ошибка создания читателя");
        return 1;
    }

    pthread_join(t_writer, NULL);
    pthread_join(t_reader, NULL);

    sem_destroy(&semaphore);

    return 0;
}
