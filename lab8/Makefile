CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread -g
SRC = main.c
TARGET = thread_app

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
	@echo "Сборка успешно завершена! Запустите программу командой: ./$(TARGET)"

clean:
	rm -f $(TARGET)
	@echo "Файлы сборки удалены."
.PHONY: all clean
