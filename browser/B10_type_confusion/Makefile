CC      = gcc
CFLAGS  = -Wall -Wextra -g
SRCS    = main.c js_object.c engine.c confusion_exploit.c
TARGET  = demo

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
