#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <mode> <file>\n", prog_name);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s +x file.txt\n", prog_name);
    fprintf(stderr, "  %s u-r file.txt\n", prog_name);
    fprintf(stderr, "  %s g+rw file.txt\n", prog_name);
    fprintf(stderr, "  %s ug+rw file.txt\n", prog_name);
    fprintf(stderr, "  %s a+rwx file.txt\n", prog_name);
    fprintf(stderr, "  %s 766 file.txt\n", prog_name);
}

int parse_octal_mode(const char *mode_str, mode_t *new_mode) {
    if (strlen(mode_str) > 4) {
        return -1;
    }
    for (int i = 0; i < strlen(mode_str); ++i) {
        if (mode_str[i] < '0' || mode_str[i] > '7') {
            return -1;
        }
    }
    *new_mode = strtol(mode_str, NULL, 8);
    return 0;
}

int parse_symbolic_mode(const char *mode_str, mode_t current_mode, mode_t *new_mode) {
    const char *p = mode_str;
    mode_t who = 0;
    char op = 0;
    mode_t perms = 0;
    while (*p && strchr("ugoa", *p)) {
        switch (*p) {
            case 'u': who |= S_IRWXU; break;
            case 'g': who |= S_IRWXG; break;
            case 'o': who |= S_IRWXO; break;
            case 'a': who = S_IRWXU | S_IRWXG | S_IRWXO; break;
        }
        p++;
    }
    if (who == 0) {
        mode_t mask = umask(0);
        umask(mask);
        who = ~mask;
    }
    if (*p == '+' || *p == '-' || *p == '=') {
        op = *p;
        p++;
    } else {
        return -1;
    }
    while (*p) {
        switch (*p) {
            case 'r': perms |= S_IRUSR | S_IRGRP | S_IROTH; break;
            case 'w': perms |= S_IWUSR | S_IWGRP | S_IWOTH; break;
            case 'x': perms |= S_IXUSR | S_IXGRP | S_IXOTH; break;
            default: return -1;
        }
        p++;
    }
    perms &= who;
    *new_mode = current_mode;
    switch (op) {
        case '+':
            *new_mode |= perms;
            break;
        case '-':
            *new_mode &= ~perms;
            break;
        case '=':
             *new_mode = (*new_mode & ~who) | perms;
            break;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *mode_str = argv[1];
    const char *file_path = argv[2];

    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        perror("Failed to get file info");
        return 1;
    }

    mode_t new_mode;
    if (isdigit(mode_str[0])) {
        if (parse_octal_mode(mode_str, &new_mode) != 0) {
            fprintf(stderr, "Invalid octal mode: %s\n", mode_str);
            return 1;
        }
    } else {
        if (parse_symbolic_mode(mode_str, file_stat.st_mode, &new_mode) != 0) {
            fprintf(stderr, "Invalid symbolic mode: %s\n", mode_str);
            return 1;
        }
    }

    if (chmod(file_path, new_mode) != 0) {
        perror("Failed to change file mode");
        return 1;
    }

    return 0;
}
