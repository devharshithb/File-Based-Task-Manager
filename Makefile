# Makefile for File-Based Task Manager (Server + Interactive Client)
# -------------------------------------------------------------------

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lpthread

# Default build target
all: server client

# Build server
server: server.c
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)

# Build client
client: client.c
	$(CC) $(CFLAGS) client.c -o client

# Clean binaries
clean:
	rm -f server client

# Run server
run-server:
	./server

# Run client
run-client:
	./client