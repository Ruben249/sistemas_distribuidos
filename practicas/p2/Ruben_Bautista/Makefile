CC = gcc
CFLAGS = -g -Wshadow -Wvla -Wall -pthread
TARGETS = P1 P2 P3

all: $(TARGETS)

P1: P1.c stub.c stub.h
	$(CC) $(CFLAGS) -o P1 P1.c stub.c

P2: P2.c stub.c stub.h
	$(CC) $(CFLAGS) -o P2 P2.c stub.c

P3: P3.c stub.c stub.h
	$(CC) $(CFLAGS) -o P3 P3.c stub.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean