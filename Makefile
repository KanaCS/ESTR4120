CC = gcc
LIB = 

all: server client

server:
	${CC} -o server_env/myftpserver myftpserver.c myftp.c ${LIB} -lpthread

client:
	${CC} -o client_env/myftpclient myftpclient.c myftp.c ${LIB}

clean:
	rm myftpserver
	rm myftpclient
