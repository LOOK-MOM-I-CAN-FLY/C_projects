#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <mode> <file>\n", prog_name);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s +x file.txt\n", prog_name);
    fprintf(stderr, "  %s u-r file.txt\n", prog_name);
    fprintf(stderr, "  %s ug+rw,o-r file.txt\n", prog_name);
    fprintf(stderr, "  %s a=rwx file.txt\n", prog_name);
    fprintf(stderr, "  %s 755 file.txt\n", prog_name);
}

int handle_octal_mode(const char *mode_str, mode_t *new_mode) {
    char *endptr;
    long mode_val = strtol(mode_str, &endptr, 8);

    if (*endptr != '\0' || mode_val < 0 || mode_val > 07777) {
        fprintf(stderr, "Invalid octal mode: %s\n", mode_str);
        return -1;
    }
    *new_mode = (mode_t)mode_val;
    return 0;
}

int handle_symbolic_mode(const char *mode_str, mode_t current_mode, mode_t *new_mode) {
    void *set = setmode(mode_str);
    if (set == NULL) {
        fprintf(stderr, "Invalid symbolic mode: %s\n", mode_str);
        return -1;
    }
    mode_t result = getmode(set, current_mode);
    free(set);
    *new_mode = result;
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *mode_str = argv[1];
    const char *file_path = argv[2];

    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        error_exit("Failed to get file info");
    }

    mode_t new_mode;
    int is_octal = 1;
    for (const char *p = mode_str; *p; ++p) {
        if (!isdigit(*p)) {
            is_octal = 0;
            break;
        }
    }
    
    int result;
    if (is_octal) {
        result = handle_octal_mode(mode_str, &new_mode);
    } else {
        result = handle_symbolic_mode(mode_str, file_stat.st_mode, &new_mode);
    }
    
    if (result != 0) {
        return EXIT_FAILURE;
    }

    if (chmod(file_path, new_mode) != 0) {
        error_exit("Failed to change file mode");
    }

    return EXIT_SUCCESS;
}
