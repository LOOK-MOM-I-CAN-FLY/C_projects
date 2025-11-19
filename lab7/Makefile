CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic
TARGET = ipc
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
