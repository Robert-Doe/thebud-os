CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c browser_kernel.c renderer.c ipc.c
OBJS = $(SRCS:.c=.o)

all: demo

demo: $(SRCS)
	$(CC) $(CFLAGS) -o demo $(SRCS)

clean:
	rm -f demo *.o

.PHONY: all clean
