CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -g -pthread -I../src/common
LDFLAGS = -pthread

SRCS = test_phase0.c ../src/common/protocol.c ../src/common/checksum.c ../src/common/utils.c
TARGET = test_phase0

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o

.PHONY: all run clean
