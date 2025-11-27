#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/types.h>
#include <inttypes.h>

#define SHM_NAME        "/ipc_shm_example_v1"
#define LOCKFILE_PATH   "/tmp/ipc_writer.lock"
#define MSG_MAX         256
#define SHM_SIZE        (sizeof(struct shared_area))

struct shared_area {
    uint64_t seq;
    pid_t sender_pid;
    time_t tsec;
    long tnsec;
    char message[MSG_MAX];
};

static int g_shm_fd = -1;
static struct shared_area *g_shm_ptr = NULL;
static int g_lockfd = -1;
static bool g_is_writer = false;

static void cleanup_and_exit(int code);
static void signal_handler(int sig);


static void sleep_us(long microseconds) {
    struct timespec req;
    req.tv_sec = microseconds / 1000000L;
    req.tv_nsec = (microseconds % 1000000L) * 1000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    }
}

static void format_timespec(time_t sec, long nsec, char *buf, size_t bufsz) {
    struct tm tm;
    if (localtime_r(&sec, &tm) == NULL) {
        snprintf(buf, bufsz, "unknown-time");
        return;
    }

    size_t used = strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);

    if (used >= bufsz) {
        if (bufsz > 0) buf[bufsz - 1] = '\0';
        return;
    }

    long msec = (long)(nsec / 1000000L);
    snprintf(buf + used, bufsz - used, ".%03ld", msec);
}


static int run_writer(void) {

    g_lockfd = open(LOCKFILE_PATH, O_CREAT | O_RDWR, 0600);
    if (g_lockfd < 0) {
        perror("open(lockfile) failed");
        return 1;
    }
    if (flock(g_lockfd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "Another writer is already running (could not acquire lock on %s). Exiting.\n", LOCKFILE_PATH);
        close(g_lockfd);
        g_lockfd = -1;
        return 2;
    }

    {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
        if (len < 0) len = 0;
        (void)ftruncate(g_lockfd, 0);
        (void)lseek(g_lockfd, 0, SEEK_SET);
        ssize_t wr = write(g_lockfd, buf, (size_t)len);
        (void)wr;
        fsync(g_lockfd);
    }

    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shm_fd < 0) {
        perror("shm_open (writer) failed");
        cleanup_and_exit(3);
        return 3;
    }
    if (ftruncate(g_shm_fd, SHM_SIZE) != 0) {
        perror("ftruncate (writer) failed");
        cleanup_and_exit(4);
        return 4;
    }
    g_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shm_ptr == MAP_FAILED) {
        perror("mmap (writer) failed");
        cleanup_and_exit(5);
        return 5;
    }

    __atomic_store_n(&g_shm_ptr->seq, (uint64_t)0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_shm_ptr->sender_pid, (pid_t)getpid(), __ATOMIC_SEQ_CST);
    g_shm_ptr->tsec = 0;
    g_shm_ptr->tnsec = 0;
    g_shm_ptr->message[0] = '\0';

    g_is_writer = true;
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Writer started (pid=%d). Shared memory name: %s\n", (int)getpid(), SHM_NAME);
    printf("Press Ctrl-C to stop writer and unlink shared memory.\n");

    uint64_t seq_local = 0;
    while (1) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            perror("clock_gettime");
            sleep(1);
            continue;
        }

        char msg[MSG_MAX];
        char timestr[64];
        format_timespec(ts.tv_sec, ts.tv_nsec, timestr, sizeof(timestr));
        int n = snprintf(msg, sizeof(msg), "From pid=%d at %s", (int)getpid(), timestr);
        if (n < 0) {
            strncpy(msg, "format-error", sizeof(msg));
            msg[sizeof(msg)-1] = '\0';
        }

        __atomic_store_n(&g_shm_ptr->sender_pid, (pid_t)getpid(), __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_shm_ptr->tsec, (time_t)ts.tv_sec, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_shm_ptr->tnsec, (long)ts.tv_nsec, __ATOMIC_SEQ_CST);

        strncpy(g_shm_ptr->message, msg, MSG_MAX - 1);
        g_shm_ptr->message[MSG_MAX - 1] = '\0';

        seq_local = __atomic_add_fetch(&g_shm_ptr->seq, (uint64_t)1, __ATOMIC_SEQ_CST);

        printf("W: seq=%" PRIu64 " wrote: %s\n", seq_local, g_shm_ptr->message);

        sleep(1);
    }

    return 0;
}

static int run_reader(void) {

    int attempts = 0;
    while (1) {
        g_shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
        if (g_shm_fd >= 0) break;
        if (errno == ENOENT) {
            if (attempts == 0) {
                fprintf(stderr, "Reader: shared memory %s not found yet. Waiting for writer to start...\n", SHM_NAME);
            }
            attempts++;
            sleep_us(200000); 
            continue;
        } else {
            perror("shm_open (reader) failed");
            return 10;
        }
    }

    g_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, g_shm_fd, 0);
    if (g_shm_ptr == MAP_FAILED) {
        perror("mmap (reader) failed");
        return 11;
    }

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Reader started (pid=%d). Attached to shared memory %s\n", (int)getpid(), SHM_NAME);

    uint64_t last_seq = __atomic_load_n(&g_shm_ptr->seq, __ATOMIC_SEQ_CST);

    while (1) {
        uint64_t cur_seq = __atomic_load_n(&g_shm_ptr->seq, __ATOMIC_SEQ_CST);
        if (cur_seq != last_seq) {

            pid_t sender_pid = __atomic_load_n(&g_shm_ptr->sender_pid, __ATOMIC_SEQ_CST);
            time_t tsec = __atomic_load_n(&g_shm_ptr->tsec, __ATOMIC_SEQ_CST);
            long tnsec = __atomic_load_n(&g_shm_ptr->tnsec, __ATOMIC_SEQ_CST);
            char message[MSG_MAX];

            memcpy(message, (const void*)g_shm_ptr->message, MSG_MAX);
            message[MSG_MAX-1] = '\0';

            struct timespec rts;
            if (clock_gettime(CLOCK_REALTIME, &rts) != 0) {
                rts.tv_sec = 0;
                rts.tv_nsec = 0;
            }
            char our_time[64], sender_time[64];
            format_timespec(rts.tv_sec, rts.tv_nsec, our_time, sizeof(our_time));
            format_timespec(tsec, tnsec, sender_time, sizeof(sender_time));

            printf("R(pid=%d) local=%s | received seq=%" PRIu64 " from pid=%d at=%s -> \"%s\"\n",
                   (int)getpid(), our_time, cur_seq, (int)sender_pid, sender_time, message);

            last_seq = cur_seq;
        }

        sleep_us(100000); 
    }

    return 0;
}

static void cleanup_and_exit(int code) {

    if (g_shm_ptr && g_shm_ptr != MAP_FAILED) {
        munmap((void*)g_shm_ptr, SHM_SIZE);
        g_shm_ptr = NULL;
    }
    if (g_shm_fd >= 0) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
    if (g_is_writer) {

        if (shm_unlink(SHM_NAME) != 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "Warning: shm_unlink failed: %s\n", strerror(errno));
            }
        } else {
            printf("Writer: shm_unlink(%s) succeeded.\n", SHM_NAME);
        }
    }
    if (g_lockfd >= 0) {

        flock(g_lockfd, LOCK_UN);
        close(g_lockfd);
        g_lockfd = -1;

    }
    exit(code);
}

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (g_is_writer) {
            printf("\nWriter received signal, cleaning up and exiting...\n");
        } else {
            printf("\nReader received signal, exiting...\n");
        }
        cleanup_and_exit(0);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s writer|reader\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "writer") == 0) {
        return run_writer();
    } else if (strcmp(argv[1], "reader") == 0) {
        return run_reader();
    } else {
        fprintf(stderr, "Unknown mode '%s'. Use 'writer' or 'reader'.\n", argv[1]);
        return 1;
    }
}
