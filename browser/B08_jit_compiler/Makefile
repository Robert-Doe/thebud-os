CC      = gcc
CFLAGS  = -Wall -Wextra -g -O0
SRCS    = main.c jit.c jit_spray_demo.c wx_demo.c
TARGET  = demo

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
