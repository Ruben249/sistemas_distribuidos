CC = gcc
CFLAGS =  -g -Wshadow -Wvla -Wall

# Default target
all: server client

# Server compilation
server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

# Client compilation  
client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)


# Clean build files
clean:
	rm -f server client


.PHONY: all clean