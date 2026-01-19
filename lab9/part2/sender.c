#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <signal.h> 

#define SHM_SIZE 1024

int shmid;
int semid;
char *shared_mem;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};

void handle_sigint(int sig) {
    printf("\nЗавершение работы sender...\n");
    
    if (shmdt(shared_mem) == -1) perror("shmdt");
    
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("shmctl");
    else printf("Shared memory removed.\n");

    if (semctl(semid, 0, IPC_RMID) == -1) perror("semctl");
    else printf("Semaphores removed.\n");

    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);

    key_t key = ftok(".", 'A');
    if (key == -1) { perror("ftok"); exit(1); }

    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    shared_mem = (char*)shmat(shmid, NULL, 0);
    if (shared_mem == (char*)(-1)) { perror("shmat"); exit(1); }

    semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) { perror("semget"); exit(1); }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) { perror("semctl"); exit(1); }

    printf("\033[0;31mSender запущен (PID: %d)\033[0m. Нажмите Ctrl+C для выхода и очистки ресурсов.\n", getpid());

    while(1) {
        if (semop(semid, &lock_res, 1) == -1) { perror("semop lock"); break; }

        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strcspn(time_str, "\n")] = 0;

        snprintf(shared_mem, SHM_SIZE, "Time: %s | Sender PID: %d", time_str, getpid());

        if (semop(semid, &rel_res, 1) == -1) { perror("semop unlock"); break; }

        sleep(3);
    }
    
    return 0;
}
