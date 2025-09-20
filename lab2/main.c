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
