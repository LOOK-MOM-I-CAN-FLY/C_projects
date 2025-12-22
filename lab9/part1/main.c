#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 256

char shared_buffer[BUFFER_SIZE];
sem_t mutex;        
sem_t data_ready;  
int counter = 0;

void* writer_thread(void* args) {
    (void)args;
    while (1) {
        sem_wait(&mutex);
        counter++;
        snprintf(shared_buffer, BUFFER_SIZE, "Запись №%d", counter);
        printf("\033[0;31m[Writer]\033[0m Записал: %s\n", shared_buffer);
        sem_post(&mutex);

        sem_post(&data_ready); 
        sleep(1);                
    }
    return NULL;
}

void* reader_thread(void* args) {
    (void)args;
    while (1) {
        sem_wait(&data_ready); 
        sem_wait(&mutex);
        pthread_t tid = pthread_self();
        printf("\033[0;32m[Reader TID: %lu]\033[0m Прочитал: %s\n",
               (unsigned long)tid, shared_buffer);
        sem_post(&mutex);
    }
    return NULL;
}

int main(void) {
    pthread_t t_writer, t_reader;

    if (sem_init(&mutex, 0, 1) != 0) { perror("sem_init mutex"); return 1; }
    if (sem_init(&data_ready, 0, 0) != 0) { perror("sem_init data_ready"); return 1; }

    if (pthread_create(&t_writer, NULL, writer_thread, NULL) != 0) { perror("create writer"); return 1; }
    if (pthread_create(&t_reader, NULL, reader_thread, NULL) != 0) { perror("create reader"); return 1; }

    pthread_join(t_writer, NULL);
    pthread_join(t_reader, NULL);

    sem_destroy(&mutex);
    sem_destroy(&data_ready);
    return 0;
}
