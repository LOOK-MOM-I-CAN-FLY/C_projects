//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

static void format_time(time_t t, char *buf, size_t bufsz) {
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void demo_pipe(void) {
    int fds[2];
    if (pipe(fds) == -1) die("pipe");

    pid_t pid = fork();
    if (pid == -1) die("fork");

    if (pid == 0) {
        close(fds[1]);

        char buf[512];
        ssize_t r = read(fds[0], buf, sizeof(buf)-1);
        if (r == -1) {
            perror("child: read");
            close(fds[0]);
            _exit(EXIT_FAILURE);
        }
        if (r == 0) {
            fprintf(stderr, "child: no data received on pipe\n");
            close(fds[0]);
            _exit(EXIT_FAILURE);
        }
        buf[r] = '\0';
        close(fds[0]);

        const unsigned int SLEEP_SECONDS = 6;
        sleep(SLEEP_SECONDS);

        time_t t_child = time(NULL);
        char timestr_child[64];
        format_time(t_child, timestr_child, sizeof(timestr_child));

        printf("=== PIPE DEMO (child) ===\n");
        printf("Child current time: %s\n", timestr_child);
        printf("Received from parent: %s\n", buf);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    } else {
        close(fds[0]);

        time_t t_parent = time(NULL);
        char timestr_parent[64];
        format_time(t_parent, timestr_parent, sizeof(timestr_parent));

        char out[512];
        int written = snprintf(out, sizeof(out), "Parent time: %s; Parent pid: %d\n",
                               timestr_parent, (int)getpid());
        if (written < 0 || (size_t)written >= sizeof(out)) {
            fprintf(stderr, "parent: message formatting error\n");
            close(fds[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }

        ssize_t w = write(fds[1], out, (size_t)written);
        if (w == -1) {
            perror("parent: write");
            close(fds[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }
        close(fds[1]);

        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
        } else {
            fprintf(stderr, "child ended abnormally\n");
        }
    }
}

static const char *make_fifo_path(void) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/demo_fifo_%d", (int)getuid());
    return path;
}

static void fifo_writer(void) {
    const char *fifo = make_fifo_path();

    if (mkfifo(fifo, 0666) == -1) {
        if (errno != EEXIST) die("mkfifo");
    }

    int fd = open(fifo, O_WRONLY);
    if (fd == -1) die("fifo writer: open O_WRONLY");

    time_t t_parent = time(NULL);
    char timestr_parent[64];
    format_time(t_parent, timestr_parent, sizeof(timestr_parent));

    char out[512];
    int written = snprintf(out, sizeof(out), "Parent time: %s; Parent pid: %d\n",
                           timestr_parent, (int)getpid());
    if (written < 0 || (size_t)written >= sizeof(out)) {
        fprintf(stderr, "fifo writer: message formatting error\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t w = write(fd, out, (size_t)written);
    if (w == -1) {
        perror("fifo writer: write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("fifo-writer: wrote message and exited\n");
}

static void fifo_reader(void) {
    const char *fifo = make_fifo_path();

    if (mkfifo(fifo, 0666) == -1) {
        if (errno != EEXIST) die("mkfifo (reader)");
    }

    int fd = open(fifo, O_RDONLY);
    if (fd == -1) die("fifo reader: open O_RDONLY");

    char buf[512];
    ssize_t r = read(fd, buf, sizeof(buf)-1);
    if (r == -1) {
        perror("fifo reader: read");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (r == 0) {
        /* writer closed with no data; make buffer empty */
        buf[0] = '\0';
        close(fd);
    } else {
        buf[r] = '\0';
        close(fd);
    }

    const unsigned int SLEEP_SECONDS = 11;
    sleep(SLEEP_SECONDS);

    time_t t_child = time(NULL);
    char timestr_child[64];
    format_time(t_child, timestr_child, sizeof(timestr_child));

    printf("=== FIFO DEMO (reader) ===\n");
    printf("Reader current time: %s\n", timestr_child);
    printf("Received from writer: %s\n", buf);
    fflush(stdout);

    if (unlink(fifo) == -1) {
        if (errno != ENOENT) {
            perror("unlink fifo");
        }
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s pipe\n"
            "  %s fifo-writer\n"
            "  %s fifo-reader\n\n"
            "Examples:\n"
            "  # pipe demo (single run):\n"
            "  %s pipe\n\n"
            "  # fifo demo (two separate processes):\n"
            "  # In terminal 1:\n"
            "  %s fifo-reader\n"
            "  # In terminal 2:\n"
            "  %s fifo-writer\n",
            prog, prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "pipe") == 0) {
        demo_pipe();
    } else if (strcmp(argv[1], "fifo-writer") == 0) {
        fifo_writer();
    } else if (strcmp(argv[1], "fifo-reader") == 0) {
        fifo_reader();
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
