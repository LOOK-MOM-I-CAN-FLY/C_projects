#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>


void print_long_format(const char *path, const char *name);
void process_path(const char *path, int show_all, int long_format, int multiple_paths);

int main(int argc, char *argv[]) {
    int opt;
    int show_all = 0;
    int long_format = 0;

    while ((opt = getopt(argc, argv, "la")) != -1) {
        switch (opt) {
            case 'l':
                long_format = 1;
                break;
            case 'a':
                show_all = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-la] [file...]\n", argv[0]);
                exit(1);
        }
    }

    int multiple_paths = (argc - optind) > 1;

    if (optind == argc) {
        process_path(".", show_all, long_format, 0);
    } else {
        for (int i = optind; i < argc; i++) {
            process_path(argv[i], show_all, long_format, multiple_paths);
            if (multiple_paths && i < argc - 1) {
                printf("\n");
            }
        }
    }

    return 0;
}

void print_long_format(const char *path, const char *name) {
    struct stat st;
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, name);

    if (lstat(full_path, &st) == -1) {
        perror("lstat");
        return;
    }

    printf((S_ISDIR(st.st_mode)) ? "d" : (S_ISLNK(st.st_mode) ? "l" : "-"));
    printf((st.st_mode & S_IRUSR) ? "r" : "-");
    printf((st.st_mode & S_IWUSR) ? "w" : "-");
    printf((st.st_mode & S_IXUSR) ? "x" : "-");
    printf((st.st_mode & S_IRGRP) ? "r" : "-");
    printf((st.st_mode & S_IWGRP) ? "w" : "-");
    printf((st.st_mode & S_IXGRP) ? "x" : "-");
    printf((st.st_mode & S_IROTH) ? "r" : "-");
    printf((st.st_mode & S_IWOTH) ? "w" : "-");
    printf((st.st_mode & S_IXOTH) ? "x" : "-");

    printf(" %2lu", (unsigned long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    if (pw != NULL) {
        printf(" %s", pw->pw_name);
    }
    if (gr != NULL) {
        printf(" %s", gr->gr_name);
    }

    printf(" %8lld", (long long)st.st_size);

    char time_buf[100];
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&st.st_mtime));
    printf(" %s ", time_buf);
}

void process_path(const char *path, int show_all, int long_format, int multiple_paths) {
    struct stat st;

    if (lstat(path, &st) == -1) {
        fprintf(stderr, "myls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        if (multiple_paths) {
            printf("%s:\n", path);
        }
        DIR *d = opendir(path);
        if (d == NULL) {
            perror("opendir");
            return;
        }

        struct dirent **namelist;
        int n = scandir(path, &namelist, NULL, alphasort);
        if (n < 0) {
            perror("scandir");
            closedir(d);
            return;
        }

        for (int i = 0; i < n; i++) {
            if (!show_all && namelist[i]->d_name[0] == '.') {
                free(namelist[i]);
                continue;
            }

            if (long_format) {
                print_long_format(path, namelist[i]->d_name);
                printf("%s\n", namelist[i]->d_name);
            } else {
                printf("%-16s", namelist[i]->d_name);
            }
            free(namelist[i]);
        }
        if (!long_format) {
            printf("\n");
        }
        free(namelist);
        closedir(d);
    } else {
        if (long_format) {
            char *dirc = strdup(path);
            char *basec = strdup(path);
            print_long_format(dirname(dirc), basename(basec));
            free(dirc);
            free(basec);
        }
        printf("%s\n", path);
    }
}
