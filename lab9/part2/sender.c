#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>

#define SHM_SIZE 1024

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct sembuf lock_res = {0, -1, 0}; 
struct sembuf rel_res = {0, 1, 0};   

int main() {

    key_t key = ftok(".", 'A');
    if (key == -1) { perror("ftok"); exit(1); }

    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    char *shared_mem = (char*)shmat(shmid, NULL, 0);
    if (shared_mem == (char*)(-1)) { perror("shmat"); exit(1); }

    int semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) { perror("semget"); exit(1); }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) { perror("semctl"); exit(1); }

    printf("\033[0;31mSender запущен (PID: %d)\033[0m. Пишу данные каждые 3 секунды...\n", getpid());

    while(1) {
        if (semop(semid, &lock_res, 1) == -1) { perror("semop lock"); exit(1); }

        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strcspn(time_str, "\n")] = 0;

        snprintf(shared_mem, SHM_SIZE, "Time: %s | Sender PID: %d", time_str, getpid());


        if (semop(semid, &rel_res, 1) == -1) { perror("semop unlock"); exit(1); }

        sleep(3);
    }

    shmdt(shared_mem);
    return 0;
}
