CC      = gcc
CFLAGS  = -Wall -Wextra -g
SRCS    = main.c value.c lexer.c parser.c eval.c confusion_demo.c
TARGET  = demo

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
