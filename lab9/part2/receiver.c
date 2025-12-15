#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <string.h>

#define SHM_SIZE 1024

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};  

int main() {

    key_t key = ftok(".", 'A');
    if (key == -1) { perror("ftok"); exit(1); }

    int shmid = shmget(key, SHM_SIZE, 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    char *shared_mem = (char*)shmat(shmid, NULL, 0);
    if (shared_mem == (char*)(-1)) { perror("shmat"); exit(1); }

    int semid = semget(key, 1, 0666);
    if (semid == -1) { perror("semget"); exit(1); }

    printf("\033[0;32mReceiver запущен (PID: %d)\033[0m. Читаю данные...\n", getpid());

    while(1) {

        if (semop(semid, &lock_res, 1) == -1) { perror("semop lock"); exit(1); }

        time_t now = time(NULL);
        char *my_time = ctime(&now);
        my_time[strcspn(my_time, "\n")] = 0;

        printf("\n--- Новые данные ---\n");
        printf("Мое время/PID: %s / %d\n", my_time, getpid());
        printf("Принято из SHM: %s\n", shared_mem);

        if (semop(semid, &rel_res, 1) == -1) { perror("semop unlock"); exit(1); }

        sleep(1);
    }

    shmdt(shared_mem);
    return 0;
}
