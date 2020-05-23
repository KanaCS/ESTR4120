CC = gcc
CFLAG = -lpthread

all: server client

server: server.c
	$(CC) -o $@ $< -std=c99

client: client.c
	$(CC) -o $@ $< -std=c99 $(CFLAG)

clean:
	rm server client