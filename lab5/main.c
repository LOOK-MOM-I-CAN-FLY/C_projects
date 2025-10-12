#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <getopt.h>

struct file_header {
    char name[256];
    struct stat metadata;
    char is_deleted; 
};


void print_help() {
    printf("Использование: ./archiver arch_name [ключ] [файл]\n");
    printf("Ключи:\n");
    printf("  -i, --input <file>    Добавить файл в архив\n");
    printf("  -e, --extract <file>  Извлечь файл из архива (с удалением из него)\n");
    printf("  -s, --stat            Показать содержимое архива\n");
    printf("  -h, --help            Показать эту справку\n");
}

void archive_file(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (arch_fd == -1) {
        perror("Не удалось открыть или создать архив");
        return;
    }

    int in_fd = open(file_name, O_RDONLY);
    if (in_fd == -1) {
        perror("Не удалось открыть входной файл");
        close(arch_fd);
        return;
    }

    struct file_header header;
    memset(&header, 0, sizeof(header));
    strncpy(header.name, file_name, sizeof(header.name) - 1);

    if (fstat(in_fd, &header.metadata) == -1) {
        perror("Не удалось получить метаданные файла");
        close(in_fd);
        close(arch_fd);
        return;
    }
    header.is_deleted = 0;

    write(arch_fd, &header, sizeof(header));

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(in_fd, buffer, sizeof(buffer))) > 0) {
        write(arch_fd, buffer, bytes_read);
    }

    printf("Файл '%s' успешно добавлен в архив '%s'.\n", file_name, archive_name);

    close(in_fd);
    close(arch_fd);
}

void extract_file(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_RDWR);
    if (arch_fd == -1) {
        perror("Не удалось открыть архив");
        return;
    }

    struct file_header header;
    ssize_t bytes_read;
    off_t header_pos = 0;
    int found = 0;

    while ((bytes_read = read(arch_fd, &header, sizeof(header))) > 0) {
        if (strcmp(header.name, file_name) == 0 && !header.is_deleted) {
            found = 1;
            int out_fd = open(header.name, O_WRONLY | O_CREAT | O_TRUNC, header.metadata.st_mode);
            if (out_fd == -1) {
                perror("Не удалось создать файл для извлечения");
                break;
            }

            char *buffer = malloc(header.metadata.st_size);
            if (!buffer) {
                 perror("Ошибка выделения памяти");
                 close(out_fd);
                 break;
            }
            
            read(arch_fd, buffer, header.metadata.st_size);
            write(out_fd, buffer, header.metadata.st_size);
            
            free(buffer);
            close(out_fd);
            
            chmod(header.name, header.metadata.st_mode);
            chown(header.name, header.metadata.st_uid, header.metadata.st_gid);
            struct utimbuf times = {header.metadata.st_atime, header.metadata.st_mtime};
            utime(header.name, &times);

            header.is_deleted = 1;
            lseek(arch_fd, header_pos, SEEK_SET);
            write(arch_fd, &header, sizeof(header));
            
            printf("Файл '%s' извлечен и помечен как удаленный в архиве.\n", file_name);
            break;
        } else {
            lseek(arch_fd, header.metadata.st_size, SEEK_CUR);
        }
        header_pos = lseek(arch_fd, 0, SEEK_CUR);
    }

    if (!found) {
        printf("Файл '%s' не найден в архиве.\n", file_name);
    }

    close(arch_fd);
}

void show_stat(const char *archive_name) {
    int arch_fd = open(archive_name, O_RDONLY);
    if (arch_fd == -1) {
        perror("Не удалось открыть архив");
        return;
    }

    struct file_header header;
    ssize_t bytes_read;
    printf("Содержимое архива '%s':\n", archive_name);
    printf("--------------------------------------------------\n");
    printf("%-30s %-10s %-20s\n", "Имя файла", "Размер (B)", "Время модификации");
    printf("--------------------------------------------------\n");

    while ((bytes_read = read(arch_fd, &header, sizeof(header))) > 0) {
        if (!header.is_deleted) {
            char time_buf[80];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&header.metadata.st_mtime));
            printf("%-30s %-10lld %-20s\n", header.name, (long long)header.metadata.st_size, time_buf);
        }
        lseek(arch_fd, header.metadata.st_size, SEEK_CUR);
    }

    close(arch_fd);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    if (argc < 3) {
        print_help();
        return 1;
    }

    const char *archive_name = argv[1];
    
    static struct option long_options[] = {
        {"input",   required_argument, 0, 'i'},
        {"extract", required_argument, 0, 'e'},
        {"stat",    no_argument,       0, 's'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 2;
    int opt;
    int option_index = 0;
    
    opt = getopt_long(argc, argv, "i:e:sh", long_options, &option_index);

    switch (opt) {
        case 'i':
            if (optarg) archive_file(archive_name, optarg);
            break;
        case 'e':
            if (optarg) extract_file(archive_name, optarg);
            break;
        case 's':
            show_stat(archive_name);
            break;
        case 'h':
            print_help();
            break;
        default:
            print_help();
            return 1;
    }
    
    return 0;
}
