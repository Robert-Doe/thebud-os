CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c css_parser.c cascade.c timing_demo.c

all: demo

demo: $(SRCS)
	$(CC) $(CFLAGS) -o demo $(SRCS)

clean:
	rm -f demo *.o

.PHONY: all clean
