CC = gcc
CFLAG = -lpthread

all: myftp myftpserver myftpclient

myftp: myftp.c
	$(CC) -o $@ $<

myftpserver: myftpserver.c
	$(CC) -o $@ $< $(CFLAG)

myftpclient: myftpclient.c
	$(CC) -o $@ $< $(CFLAG)

clean:
	rm myftp myftpserver myftpclient
