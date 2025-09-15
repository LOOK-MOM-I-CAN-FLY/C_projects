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
