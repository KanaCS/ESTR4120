CC = gcc
LIB = 

all: server client

server:
	${CC} -o myftpserver myftpserver.c myftp.c ${LIB} -lpthread

client:
	${CC} -o myftpclient myftpclient.c myftp.c ${LIB}

clean:
	rm myftpserver
	rm myftpclient
