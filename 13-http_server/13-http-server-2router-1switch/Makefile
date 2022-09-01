TARGET = http-server
all: $(TARGET) cleano

CC = gcc -w
LD = gcc

CFLAGS = -g -Wall -Wno-unused-variable -Wno-unused-function -Iinclude
LDFLAGS = -L.

LIBS = -lpthread

HDRS = ./include/*.h
 
SRCS = $(wildcard *.c)

OBJS = $(patsubst %.c,%.o,$(SRCS))

$(OBJS) : %.o : %.c include/*.h
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(LIBIP) $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS) 

clean:
	rm -f *.o $(TARGET)

cleano:
	rm -f *.o