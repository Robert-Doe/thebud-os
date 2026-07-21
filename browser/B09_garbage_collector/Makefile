CC      = gcc
CFLAGS  = -Wall -Wextra -g
SRCS    = main.c heap.c gc.c uaf_demo.c
TARGET  = demo

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
