CC = gcc
CFLAGS = -Wall -g -std=gnu99 -pthread

SOURCES = l25_tcp_server.c
HEADERS = l25_tcp_server.h
TARGET = l25_tcp_server

.PHONY: all clean tags submit

objects = $(SOURCES:.c=.o)

proxy: $(objects) 
	$(CC) $(CFLAGS) -o $(TARGET) $(objects)

all: $(TARGET)

clean: 
	@rm -f *.o $(TARGET) 

tags:
	@ctags $(SOURCES)
