#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <regex.h>
#include <stdlib.h>


#define NO_FLAGS 0
#define N_FLAG 1
#define B_FLAG 2
#define E_FLAG 4

int mycat_process_file(const char *file_name, int flags);
void mycat_process_stdin(int flags);
int mycat_main(int argc, char *argv[]);


int mygrep_search_in_file(const char *pattern, const char *file_name, int multiple_files);
void mygrep_search_in_stdin(const char *pattern);
int mygrep_main(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    if (strstr(argv[0], "mycat")) {
        return mycat_main(argc, argv);
    } else if (strstr(argv[0], "mygrep")) {
        return mygrep_main(argc, argv);
    } else {
        fprintf(stderr, "Executable must be named 'mycat' or 'mygrep'\n");
        return 1;
    }
}

int mycat_main(int argc, char *argv[]) {
    int opt;
    int flags = NO_FLAGS;
    int exit_status = 0;

    struct option long_options[] = {
        {"number", no_argument, 0, 'n'},
        {"number-nonblank", no_argument, 0, 'b'},
        {"show-ends", no_argument, 0, 'E'},
        {0, 0, 0, 0}
    };

    optind = 1; // Reset getopt
    while ((opt = getopt_long(argc, argv, "nbE", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                flags |= N_FLAG;
                break;
            case 'b':
                flags |= B_FLAG;
                break;
            case 'E':
                flags |= E_FLAG;
                break;
            default:
                fprintf(stderr, "Usage: %s [-n] [-b] [-E] [file...]\n", argv[0]);
                return 1;
        }
    }

    if (optind == argc) {
        mycat_process_stdin(flags);
    } else {
        for (int i = optind; i < argc; i++) {
            exit_status |= mycat_process_file(argv[i], flags);
        }
    }

    return exit_status;
}

int mycat_process_file(const char *file_name, int flags) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror(file_name);
        return 1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int line_number = 1;

    while ((read = getline(&line, &len, file)) != -1) {
        int is_empty = (line[0] == '\n');

        if ((flags & B_FLAG) && !is_empty) {
            printf("%6d\t", line_number++);
        } else if ((flags & N_FLAG) && !(flags & B_FLAG)) {
            printf("%6d\t", line_number++);
        }

        if (flags & E_FLAG) {
            if (line[read - 1] == '\n') {
                line[read - 1] = '$';
                printf("%s\n", line);
            } else {
                printf("%s$\n", line);
            }
        } else {
            printf("%s", line);
        }
    }

    free(line);
    fclose(file);
    return 0;
}

void mycat_process_stdin(int flags) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int line_number = 1;

    while ((read = getline(&line, &len, stdin)) != -1) {
        int is_empty = (line[0] == '\n');

        if ((flags & B_FLAG) && !is_empty) {
            printf("%6d\t", line_number++);
        } else if ((flags & N_FLAG) && !(flags & B_FLAG)) {
            printf("%6d\t", line_number++);
        }

        if (flags & E_FLAG) {
            if (line[read - 1] == '\n') {
                line[read - 1] = '$';
                printf("%s\n", line);
            } else {
                printf("%s$\n", line);
            }
        } else {
            printf("%s", line);
        }
    }

    free(line);
}


int mygrep_main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s pattern [file...]\n", argv[0]);
        return 1;
    }

    const char *pattern = argv[1];
    int exit_status = 0;
    int multiple_files = (argc > 3);

    if (argc == 2) {
        mygrep_search_in_stdin(pattern);
    } else {
        for (int i = 2; i < argc; i++) {
            exit_status |= mygrep_search_in_file(pattern, argv[i], multiple_files);
        }
    }

    return exit_status;
}

int mygrep_search_in_file(const char *pattern, const char *file_name, int multiple_files) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror(file_name);
        return 1;
    }

    char *line = NULL;
    size_t len = 0;
    regex_t regex;

    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        fprintf(stderr, "Could not compile regex\n");
        fclose(file);
        return 1;
    }

    while (getline(&line, &len, file) != -1) {
        if (regexec(&regex, line, 0, NULL, 0) == 0) {
            if (multiple_files) {
                printf("%s:%s", file_name, line);
            } else {
                printf("%s", line);
            }
        }
    }

    regfree(&regex);
    free(line);
    fclose(file);
    return 0;
}

void mygrep_search_in_stdin(const char *pattern) {
    char *line = NULL;
    size_t len = 0;
    regex_t regex;

    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        fprintf(stderr, "Could not compile regex\n");
        return;
    }

    while (getline(&line, &len, stdin) != -1) {
        if (regexec(&regex, line, 0, NULL, 0) == 0) {
            printf("%s", line);
        }
    }

    regfree(&regex);
    free(line);
}
