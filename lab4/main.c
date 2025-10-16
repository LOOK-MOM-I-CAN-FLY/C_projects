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
    if (mode_str == NULL || *mode_str == '\0') {
        fprintf(stderr, "Invalid symbolic mode: empty\n");
        return -1;
    }

    mode_t result_mode = current_mode;
    const char *p = mode_str;

    while (*p) {
        /* Parse who: [ugoa]* or empty (defaults to 'a') */
        int affect_u = 0, affect_g = 0, affect_o = 0;
        const char *who_start = p;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            if (*p == 'u') affect_u = 1;
            else if (*p == 'g') affect_g = 1;
            else if (*p == 'o') affect_o = 1;
            else if (*p == 'a') affect_u = affect_g = affect_o = 1;
            p++;
        }
        if (p == who_start) {
            /* no who specified -> default to 'a' */
            affect_u = affect_g = affect_o = 1;
        }

        /* Parse op */
        char op = *p;
        if (op != '+' && op != '-' && op != '=') {
            fprintf(stderr, "Invalid symbolic operator near: %s\n", p);
            return -1;
        }
        p++;

        /* Parse perms: [rwx]*; for '=' empty is allowed (clears all), for '+'/'-' empty is invalid */
        int have_perm = 0;
        int perm_r = 0, perm_w = 0, perm_x = 0;
        while (*p == 'r' || *p == 'w' || *p == 'x') {
            have_perm = 1;
            if (*p == 'r') perm_r = 1;
            else if (*p == 'w') perm_w = 1;
            else if (*p == 'x') perm_x = 1;
            p++;
        }
        if ((op == '+' || op == '-') && !have_perm) {
            fprintf(stderr, "Invalid symbolic mode: missing permissions after '%c'\n", op);
            return -1;
        }

        /* Build bitmask per class */
        mode_t mask_u = 0, mask_g = 0, mask_o = 0;
        if (perm_r) {
            if (affect_u) mask_u |= S_IRUSR;
            if (affect_g) mask_g |= S_IRGRP;
            if (affect_o) mask_o |= S_IROTH;
        }
        if (perm_w) {
            if (affect_u) mask_u |= S_IWUSR;
            if (affect_g) mask_g |= S_IWGRP;
            if (affect_o) mask_o |= S_IWOTH;
        }
        if (perm_x) {
            if (affect_u) mask_u |= S_IXUSR;
            if (affect_g) mask_g |= S_IXGRP;
            if (affect_o) mask_o |= S_IXOTH;
        }

        if (op == '+') {
            result_mode |= mask_u | mask_g | mask_o;
        } else if (op == '-') {
            result_mode &= ~(mask_u | mask_g | mask_o);
        } else if (op == '=') {
            /* Clear r/w/x for affected classes, then set */
            if (affect_u) {
                result_mode &= ~(S_IRUSR | S_IWUSR | S_IXUSR);
                result_mode |= mask_u;
            }
            if (affect_g) {
                result_mode &= ~(S_IRGRP | S_IWGRP | S_IXGRP);
                result_mode |= mask_g;
            }
            if (affect_o) {
                result_mode &= ~(S_IROTH | S_IWOTH | S_IXOTH);
                result_mode |= mask_o;
            }
        }

        /* Next clause separated by comma */
        if (*p == ',') {
            p++;
            if (*p == '\0') {
                fprintf(stderr, "Invalid symbolic mode: trailing comma\n");
                return -1;
            }
        } else if (*p != '\0') {
            fprintf(stderr, "Invalid symbolic mode near: %s\n", p);
            return -1;
        }
    }

    *new_mode = result_mode;
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
