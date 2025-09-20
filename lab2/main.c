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
