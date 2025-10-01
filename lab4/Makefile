TARGET = mychmod
CC = gcc
CFLAGS = -Wall -Wextra -o
SRCS = main.c
all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(TARGET) $(SRCS)
clean:
	rm -f $(TARGET)
re: clean all
