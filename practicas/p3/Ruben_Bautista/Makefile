CC = gcc
CFLAGS = -g -Wshadow -Wvla -Wall -pthread
TARGETS = client server

all: $(TARGETS)

client: client.c stub.c stub.h
	$(CC) $(CFLAGS) -o client client.c stub.c

server: server.c stub.c stub.h
	$(CC) $(CFLAGS) -o server server.c stub.c


clean:
	rm -f $(TARGETS)

.PHONY: all clean