CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lrt -lpthread
TARGET = atomsync

SRCS = src/main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) 