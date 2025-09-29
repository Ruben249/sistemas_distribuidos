# Makefile for Simple TCP Server/Client
# Systems Distributed and Concurrent

CC = gcc
CFLAGS = -I. -Wall -Wextra -std=c99
LDFLAGS = 
DEBUG_FLAGS = -DDEBUG
RELEASE_FLAGS = 

# Default target
all: server client

# Server compilation
server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

# Client compilation  
client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

# Debug versions
debug: CFLAGS += $(DEBUG_FLAGS)
debug: server client

# Clean build files
clean:
	rm -f server client

# Run server (for testing)
run-server: server
	./server

# Run client (for testing)  
run-client: client
	./client

.PHONY: all clean debug run-server run-client