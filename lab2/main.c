#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <getopt.h>

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_CYAN    "\x1b[36m"

void process_path(const char *path, int show_all, int long_format);
void print_long_format(const char *path, const char *name);
void print_file_type(mode_t mode);
void print_permissions(mode_t mode);

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
                fprintf(stderr, "Использование: %s [-la] [файл...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        process_path(".", show_all, long_format);
    } else {
        for (int i = optind; i < argc; i++) {
            process_path(argv[i], show_all, long_format);
        }
    }

    return 0;
}

void process_path(const char *path, int show_all, int long_format) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror(path);
        return;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        if (long_format) {
            print_long_format(".", path); 
        } else {
            if (S_ISLNK(st.st_mode)) {
                printf("%s%s%s\n", COLOR_CYAN, path, COLOR_RESET);
            } else if (st.st_mode & S_IXUSR) {
                printf("%s%s%s\n", COLOR_GREEN, path, COLOR_RESET);
            } else {
                printf("%s\n", path);
            }
        }
        return;
    }
    
    struct dirent **namelist;
    int n;

    n = scandir(path, &namelist, NULL, alphasort);
    if (n < 0) {
        perror("scandir");
        return;
    }

    for (int i = 0; i < n; i++) {
        if (!show_all && namelist[i]->d_name[0] == '.') {
            free(namelist[i]);
            continue;
        }

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, namelist[i]->d_name);

        if (long_format) {
            print_long_format(fullpath, namelist[i]->d_name);
        } else {
            struct stat sb;
            if (lstat(fullpath, &sb) == -1) {
                perror("lstat");
                free(namelist[i]);
                continue;
            }

            if (S_ISDIR(sb.st_mode)) {
                printf("%s%s%s  ", COLOR_BLUE, namelist[i]->d_name, COLOR_RESET);
            } else if (S_ISLNK(sb.st_mode)) {
                printf("%s%s%s  ", COLOR_CYAN, namelist[i]->d_name, COLOR_RESET);
            } else if (sb.st_mode & S_IXUSR) {
                printf("%s%s%s  ", COLOR_GREEN, namelist[i]->d_name, COLOR_RESET);
            } else {
                printf("%s  ", namelist[i]->d_name);
            }
        }
        free(namelist[i]);
    }

    if (!long_format) {
        printf("\n");
    }

    free(namelist);
}

void print_long_format(const char *path, const char *name) {
    struct stat sb;

    const char *path_to_stat = path;
    struct stat path_stat;
    
    if (lstat(path, &path_stat) != 0) {
        path_to_stat = name;
    }


    if (lstat(path_to_stat, &sb) == -1) {
        perror(path_to_stat);
        return;
    }

    print_file_type(sb.st_mode);
    print_permissions(sb.st_mode);
    printf(" %2lu", sb.st_nlink);

    struct passwd *pw = getpwuid(sb.st_uid);
    struct group *gr = getgrgid(sb.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");

    printf(" %7lld", (long long)sb.st_size);

    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&sb.st_mtime));
    printf(" %s ", time_buf);

    if (S_ISDIR(sb.st_mode)) {
        printf("%s%s%s", COLOR_BLUE, name, COLOR_RESET);
    } else if (S_ISLNK(sb.st_mode)) {
        char link_target[1024];
        ssize_t len = readlink(path_to_stat, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0';
            printf("%s%s%s -> %s", COLOR_CYAN, name, COLOR_RESET, link_target);
        } else {
            printf("%s%s%s", COLOR_CYAN, name, COLOR_RESET);
        }
    } else if (sb.st_mode & S_IXUSR) {
        printf("%s%s%s", COLOR_GREEN, name, COLOR_RESET);
    } else {
        printf("%s", name);
    }

    printf("\n");
}

void print_file_type(mode_t mode) {
    switch (mode & S_IFMT) {
        case S_IFREG:  printf("-"); break;
        case S_IFDIR:  printf("d"); break;
        case S_IFLNK:  printf("l"); break;
        case S_IFCHR:  printf("c"); break;
        case S_IFBLK:  printf("b"); break;
        case S_IFIFO:  printf("p"); break;
        case S_IFSOCK: printf("s"); break;
        default:       printf("?"); break;
    }
}

void print_permissions(mode_t mode) {
    printf((mode & S_IRUSR) ? "r" : "-");
    printf((mode & S_IWUSR) ? "w" : "-");
    printf((mode & S_IXUSR) ? "x" : "-");
    printf((mode & S_IRGRP) ? "r" : "-");
    printf((mode & S_IWGRP) ? "w" : "-");
    printf((mode & S_IXGRP) ? "x" : "-");
    printf((mode & S_IROTH) ? "r" : "-");
    printf((mode & S_IWOTH) ? "w" : "-");
    printf((mode & S_IXOTH) ? "x" : "-");
}
