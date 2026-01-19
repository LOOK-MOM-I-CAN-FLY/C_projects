#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h> 

#define SHM_SIZE 1024

char *shared_mem; 

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};

void handle_sigint(int sig) {
    printf("\nЗавершение работы receiver...\n");
    if (shmdt(shared_mem) == -1) perror("shmdt");
    else printf("Detached from shared memory.\n");
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);

    key_t key = ftok(".", 'A');
    if (key == -1) { perror("ftok"); exit(1); }

    int shmid = shmget(key, SHM_SIZE, 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    shared_mem = (char*)shmat(shmid, NULL, 0);
    if (shared_mem == (char*)(-1)) { perror("shmat"); exit(1); }

    int semid = semget(key, 1, 0666);
    if (semid == -1) { perror("semget"); exit(1); }

    printf("\033[0;32mReceiver запущен (PID: %d)\033[0m. Нажмите Ctrl+C для выхода.\n", getpid());

    while(1) {
        if (semop(semid, &lock_res, 1) == -1) { perror("semop lock"); break; }

        time_t now = time(NULL);
        char *my_time = ctime(&now);
        my_time[strcspn(my_time, "\n")] = 0;

        printf("\n--- Новые данные ---\n");
        printf("Мое время/PID: %s / %d\n", my_time, getpid());
        printf("Принято из SHM: %s\n", shared_mem);

        if (semop(semid, &rel_res, 1) == -1) { perror("semop unlock"); break; }

        sleep(1);
    }
    return 0;
}
