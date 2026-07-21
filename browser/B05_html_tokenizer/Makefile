CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c tokenizer.c dom.c

all: demo

demo: $(SRCS)
	$(CC) $(CFLAGS) -o demo $(SRCS)

clean:
	rm -f demo *.o

.PHONY: all clean
