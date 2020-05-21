CC = gcc
CFLAG = -lpthread -std=c99

all: server client

server: server.c
	$(CC) -o $@ $< -std=c99

client: client.c
	$(CC) -o $@ $< -std=c99

clean:
	rm server client