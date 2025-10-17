#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <getopt.h>
#include <errno.h>

struct file_header {
    char name[256];
    struct stat metadata;
    char is_deleted;
};

#define MAX_FILE_SIZE (1024LL * 1024LL * 1024LL)

void print_help() {
    printf("Использование: ./archiver arch_name [ключ] [файл]\n");
    printf("Ключи:\n");
    printf("  -i, --input <file>    Добавить файл в архив\n");
    printf("  -e, --extract <file>  Извлечь файл из архива (с удалением из него)\n");
    printf("  -s, --stat            Показать содержимое архива\n");
    printf("  -h, --help            Показать эту справку\n");
}

/* Сжимает архив: копирует только ненужные (is_deleted == 0) записи
   в временный файл и переименовывает его в исходный */
int compact_archive(const char *archive_name) {
    int in_fd = open(archive_name, O_RDONLY);
    if (in_fd == -1) {
        perror("compact: не удалось открыть архив для чтения");
        return -1;
    }

    /* temp template: "<archive_name>.tmpXXXXXX" */
    size_t tlen = strlen(archive_name) + 12;
    char *tmp_name = malloc(tlen);
    if (!tmp_name) { close(in_fd); return -1; }
    snprintf(tmp_name, tlen, "%s.tmpXXXXXX", archive_name);

    int tmp_fd = mkstemp(tmp_name);
    if (tmp_fd == -1) {
        perror("compact: не удалось создать временный файл");
        free(tmp_name);
        close(in_fd);
        return -1;
    }

    /* попытаться применить права исходного файла к временному */
    struct stat arch_st;
    if (fstat(in_fd, &arch_st) == 0) {
        fchmod(tmp_fd, arch_st.st_mode);
    }

    struct file_header header;
    ssize_t r;
    char buf[4096];

    while ((r = read(in_fd, &header, sizeof(header))) == sizeof(header)) {
        off_t remaining = header.metadata.st_size;

        if (!header.is_deleted) {
            /* Переписать заголовок */
            if (write(tmp_fd, &header, sizeof(header)) != sizeof(header)) {
                perror("compact: ошибка записи заголовка во временный файл");
                close(in_fd);
                close(tmp_fd);
                unlink(tmp_name);
                free(tmp_name);
                return -1;
            }

            /* Копировать данные порциями */
            while (remaining > 0) {
                ssize_t to_read = (remaining > (off_t)sizeof(buf)) ? sizeof(buf) : (ssize_t)remaining;
                ssize_t br = read(in_fd, buf, to_read);
                if (br <= 0) {
                    if (br == -1) perror("compact: ошибка чтения данных из архива");
                    else fprintf(stderr, "compact: неожиданное EOF при чтении данных\n");
                    close(in_fd);
                    close(tmp_fd);
                    unlink(tmp_name);
                    free(tmp_name);
                    return -1;
                }
                ssize_t bw = write(tmp_fd, buf, br);
                if (bw != br) {
                    perror("compact: ошибка записи данных во временный файл");
                    close(in_fd);
                    close(tmp_fd);
                    unlink(tmp_name);
                    free(tmp_name);
                    return -1;
                }
                remaining -= br;
            }
        } else {
            /* Пропустить данные удалённой записи (lseek быстрее, если поддерживается) */
            if (lseek(in_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                /* на случай, если lseek не сработал, прочитаем и пропустим вручную */
                off_t skip = header.metadata.st_size;
                while (skip > 0) {
                    ssize_t to_read = (skip > (off_t)sizeof(buf)) ? sizeof(buf) : (ssize_t)skip;
                    ssize_t br = read(in_fd, buf, to_read);
                    if (br <= 0) break;
                    skip -= br;
                }
            }
        }
    }

    if (r == -1) {
        perror("compact: ошибка чтения архива");
        close(in_fd);
        close(tmp_fd);
        unlink(tmp_name);
        free(tmp_name);
        return -1;
    }

    /* flush & close */
    fsync(tmp_fd);
    close(tmp_fd);
    close(in_fd);

    /* заменить оригинал временным файлом */
    if (rename(tmp_name, archive_name) == -1) {
        perror("compact: не удалось заменить архив временным файлом");
        unlink(tmp_name);
        free(tmp_name);
        return -1;
    }

    free(tmp_name);
    return 0;
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

    if (strlen(file_name) >= sizeof(header.name)) {
        printf("Ошибка: имя файла '%s' слишком длинное (максимум %zu символов)\n",
               file_name, sizeof(header.name) - 1);
        close(in_fd);
        close(arch_fd);
        return;
    }
    memset(&header, 0, sizeof(header));
    strncpy(header.name, file_name, sizeof(header.name) - 1);

    if (fstat(in_fd, &header.metadata) == -1) {
        perror("Не удалось получить метаданные файла");
        close(in_fd);
        close(arch_fd);
        return;
    }
    header.is_deleted = 0;

    if (write(arch_fd, &header, sizeof(header)) != sizeof(header)) {
        perror("Ошибка записи заголовка в архив");
        close(in_fd);
        close(arch_fd);
        return;
    }

    char buffer[4096];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(in_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(arch_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Ошибка записи данных в архив");
            close(in_fd);
            close(arch_fd);
            return;
        }
    }

    if (bytes_read == -1) {
        perror("Ошибка чтения исходного файла");
        close(in_fd);
        close(arch_fd);
        return;
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
    int found = 0;

    while (1) {
        off_t header_start = lseek(arch_fd, 0, SEEK_CUR);
        if (header_start == -1) {
            perror("Ошибка определения позиции в архиве");
            break;
        }

        bytes_read = read(arch_fd, &header, sizeof(header));
        if (bytes_read == 0) break; /* EOF */
        if (bytes_read != sizeof(header)) {
            perror("Ошибка чтения заголовка из архива");
            break;
        }

        /* если совпадает имя и не помечен как удалённый — извлекаем */
        if (strcmp(header.name, file_name) == 0 && !header.is_deleted) {
            found = 1;

            if (header.metadata.st_size > MAX_FILE_SIZE) {
                printf("Файл '%s' слишком большой для извлечения (размер: %lld байт)\n",
                       file_name, (long long)header.metadata.st_size);
                /* пропустим данные и выйдем */
                if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка пропуска данных большого файла");
                }
                break;
            }

            int out_fd = open(header.name, O_WRONLY | O_CREAT | O_TRUNC, header.metadata.st_mode);
            if (out_fd == -1) {
                perror("Не удалось создать файл для извлечения");
                /* пропустим данные */
                if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка пропуска данных при неудачном создании файла");
                }
                break;
            }

            char buffer[4096];
            off_t remaining = header.metadata.st_size;
            ssize_t br, bw;
            int read_error = 0;

            while (remaining > 0) {
                ssize_t to_read = (remaining > (off_t)sizeof(buffer)) ? sizeof(buffer) : (ssize_t)remaining;
                br = read(arch_fd, buffer, to_read);
                if (br <= 0) {
                    if (br == -1) perror("Ошибка чтения из архива при извлечении");
                    else fprintf(stderr, "Неожиданный EOF при извлечении\n");
                    read_error = 1;
                    break;
                }
                bw = write(out_fd, buffer, br);
                if (bw != br) {
                    perror("Ошибка записи извлеченного файла");
                    read_error = 1;
                    break;
                }
                remaining -= br;
            }

            if (read_error) {
                close(out_fd);
                break;
            }

            close(out_fd);

            /* Восстановить атрибуты */
            if (chmod(header.name, header.metadata.st_mode) == -1) {
                perror("Предупреждение: не удалось восстановить права доступа");
            }
            if (chown(header.name, header.metadata.st_uid, header.metadata.st_gid) == -1) {
                /* Часто не удаётся если не root — это не критично */
                /* perror("Предупреждение: не удалось восстановить владельца"); */
            }

            struct utimbuf times = {header.metadata.st_atime, header.metadata.st_mtime};
            if (utime(header.name, &times) == -1) {
                perror("Предупреждение: не удалось восстановить время модификации");
            }

            /* Пометить запись как удалённую в архиве */
            header.is_deleted = 1;
            if (lseek(arch_fd, header_start, SEEK_SET) == -1) {
                perror("Ошибка позиционирования в архиве для пометки удаления");
            } else if (write(arch_fd, &header, sizeof(header)) != sizeof(header)) {
                perror("Ошибка записи пометки удаления в архив");
            }

            /* Закрываем дескриптор и запускаем компактацию архива */
            close(arch_fd);
            if (compact_archive(archive_name) == -1) {
                fprintf(stderr, "Предупреждение: не удалось сжать архив после удаления. Архив корректно помечен, но размер может остаться прежним.\n");
            }

            printf("Файл '%s' извлечен и удалён из архива.\n", file_name);
            return;
        } else {
            /* Пропустить данные этой записи и продолжить */
            if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                perror("Ошибка позиционирования в архиве при пропуске записи");
                break;
            }
        }
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
        if (bytes_read != sizeof(header)) {
            perror("Ошибка чтения заголовка при просмотре архива");
            break;
        }
        if (!header.is_deleted) {
            char time_buf[80];
            struct tm *tm = localtime(&header.metadata.st_mtime);
            if (tm) strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm);
            else strncpy(time_buf, "unknown", sizeof(time_buf));
            printf("%-30s %-10lld %-20s\n", header.name, (long long)header.metadata.st_size, time_buf);
        }
        if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
            perror("Ошибка позиционирования в архиве при просмотре");
            break;
        }
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
