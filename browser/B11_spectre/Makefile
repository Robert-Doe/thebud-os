CC      = gcc
CFLAGS  = -Wall -Wextra -g -O0
SRCS    = main.c cache_timing.c spectre_demo.c
TARGET  = demo

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
