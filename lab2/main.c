
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


typedef struct {
    char name[256]; 
    struct stat st; 
} FileEntry;


void process_path(const char *path, int show_all, int long_format);
void print_long_format(const char *path, const FileEntry *entry);
void print_short_format(const FileEntry *entry);
void print_file_type(mode_t mode);
void print_permissions(mode_t mode);
int compare_entries(const void *a, const void *b);

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
            
            if (i > optind && argc - optind > 1) {
                printf("\n");
            }
            process_path(argv[i], show_all, long_format);
        }
    }

    return 0;
}


int compare_entries(const void *a, const void *b) {
    const FileEntry *entry_a = (const FileEntry *)a;
    const FileEntry *entry_b = (const FileEntry *)b;
    return strcmp(entry_a->name, entry_b->name);
}

void process_path(const char *path, int show_all, int long_format) {
    struct stat path_st;
    if (lstat(path, &path_st) == -1) {
        perror(path);
        return;
    }

    
    if (!S_ISDIR(path_st.st_mode)) {
        FileEntry entry;
        strncpy(entry.name, path, sizeof(entry.name) - 1);
        entry.st = path_st;

        if (long_format) {
            print_long_format(path, &entry);
        } else {
            print_short_format(&entry);
            printf("\n");
        }
        return;
    }
    
    
    
    
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror(path);
        return;
    }

    FileEntry *entries = NULL;
    int count = 0;
    struct dirent *dirent_p;

    
    while ((dirent_p = readdir(dir)) != NULL) {
        if (!show_all && dirent_p->d_name[0] == '.') {
            continue;
        }

        
        entries = realloc(entries, (count + 1) * sizeof(FileEntry));
        if (entries == NULL) {
            perror("realloc");
            closedir(dir);
            exit(EXIT_FAILURE);
        }

        
        FileEntry *current_entry = &entries[count];
        strncpy(current_entry->name, dirent_p->d_name, sizeof(current_entry->name) - 1);
        
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dirent_p->d_name);

        if (lstat(fullpath, &current_entry->st) == -1) {
            perror(fullpath);
            
            continue; 
        }
        count++;
    }
    closedir(dir);

    
    qsort(entries, count, sizeof(FileEntry), compare_entries);

    
    for (int i = 0; i < count; i++) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entries[i].name);
        if (long_format) {
            print_long_format(fullpath, &entries[i]);
        } else {
            print_short_format(&entries[i]);
        }
    }

    if (!long_format && count > 0) {
        printf("\n");
    }

    free(entries); 
}

void print_short_format(const FileEntry *entry) {
    mode_t mode = entry->st.st_mode;
    const char *name = entry->name;

    if (S_ISDIR(mode)) {
        printf("%s%s%s  ", COLOR_BLUE, name, COLOR_RESET);
    } else if (S_ISLNK(mode)) {
        printf("%s%s%s  ", COLOR_CYAN, name, COLOR_RESET);
    } else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) { 
        printf("%s%s%s  ", COLOR_GREEN, name, COLOR_RESET);
    } else {
        printf("%s  ", name);
    }
}

void print_long_format(const char *path, const FileEntry *entry) {
    struct stat sb = entry->st;
    const char *name = entry->name;
    
    print_file_type(sb.st_mode);
    print_permissions(sb.st_mode);

    
    printf(" %2lu", (unsigned long)sb.st_nlink);

    struct passwd *pw = getpwuid(sb.st_uid);
    struct group *gr = getgrgid(sb.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "uid", gr ? gr->gr_name : "gruid");

    printf(" %7lld", (long long)sb.st_size);

    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&sb.st_mtime));
    printf(" %s ", time_buf);

    
    if (S_ISDIR(sb.st_mode)) {
        printf("%s%s%s", COLOR_BLUE, name, COLOR_RESET);
    } else if (S_ISLNK(sb.st_mode)) {
        char link_target[1024];
        ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0';
            printf("%s%s%s -> %s", COLOR_CYAN, name, COLOR_RESET, link_target);
        } else {
            printf("%s%s%s", COLOR_CYAN, name, COLOR_RESET);
        }
    } else if (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
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
