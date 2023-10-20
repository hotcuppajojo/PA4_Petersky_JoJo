CC=gcc
CFLAGS=-Wall -pthread
TARGET=mymalloc

$(TARGET): mymalloc.c
	$(CC) $(CFLAGS) -o $(TARGET) mymalloc.c

.PHONY: clean
clean:
	rm -f $(TARGET)
