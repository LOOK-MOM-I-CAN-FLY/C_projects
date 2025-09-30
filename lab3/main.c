#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

// Прототип функции для обработчика atexit
void exit_handler(void);

// Прототип функции для обработчика сигнала SIGINT
void sigint_handler(int signum);

// Прототип функции для обработчика сигнала SIGTERM
void sigterm_handler(int signum, siginfo_t *info, void *context);

int main() {
    // 2. Использование своего обработчика atexit()
    if (atexit(exit_handler) != 0) {
        perror("atexit");
        return 1;
    }

    // 5. Переопределение обработчика сигнала SIGINT при помощи системного вызова signal()
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    // 5. Переопределение обработчика сигнала SIGTERM при помощи вызова sigaction()
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO; // Используем sa_sigaction вместо sa_handler
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("Основная программа: PID %d, PPID %d\n", getpid(), getppid());
    printf("Для проверки обработчика SIGINT, нажмите Ctrl+C\n");
    printf("Для проверки обработчика SIGTERM, выполните в другом терминале: kill %d\n\n", getpid());


    // 1. Вызов fork()
    pid_t pid = fork();

    // Обработка ошибки вызова fork()
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    // Код, выполняемый дочерним процессом
    else if (pid == 0) {
        printf("Это дочерний процесс.\n");
        // 3. Дочерний процесс выводит свой PID и PPID
        printf("Дочерний процесс: PID %d, PPID %d\n", getpid(), getppid());
        printf("Дочерний процесс: засыпаю на 3 секунды...\n");
        sleep(3);
        printf("Дочерний процесс: завершаю работу с кодом 10.\n");
        exit(10);
    }
    // Код, выполняемый родительским процессом
    else {
        FILE *f = fopen("pid.txt", "w");
        if (f) {
            fprintf(f, "%d", getpid());
            fclose(f);
        }

        printf("Это родительский процесс.\n");
        // 3. Родительский процесс выводит свой PID и PPID
        printf("Родительский процесс: PID %d, PPID %d\n", getpid(), getppid());

        // 4. Родительский процесс ожидает дочерний
        int status;
        printf("Родительский процесс: ожидаю завершения дочернего процесса...\n");
        wait(&status);

        // 4. ... и выводит в консоль его код завершения
        if (WIFEXITED(status)) {
            printf("Родительский процесс: дочерний процесс завершился с кодом %d.\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Родительский процесс: дочерний процесс был завершен сигналом %d.\n", WTERMSIG(status));
        }
        printf("Родительский процесс: засыпаю на 20 секунд...\n");
        sleep(20);
    }

    printf("Основная программа: PID %d завершает работу.\n", getpid());

    return 0;
}

// Реализация обработчика, регистрируемого через atexit()
void exit_handler(void) {
    printf("--- [atexit handler]: Процесс с PID %d завершает работу. ---\n", getpid());
}

// Реализация обработчика для сигнала SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    // Используем write для безопасности в обработчиках сигналов
    char msg[] = "--- [signal handler]: Получен сигнал SIGINT (Ctrl+C). Игнорирую. ---\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

// Реализация обработчика для сигнала SIGTERM
void sigterm_handler(int signum, siginfo_t *info, void *context) {
    char msg[100];
    sprintf(msg, "--- [sigaction handler]: Получен сигнал SIGTERM (%d). Завершаю работу. ---\n", signum);
    write(STDOUT_FILENO, msg, strlen(msg));
    exit(1); // Завершаем программу после получения SIGTERM
}
