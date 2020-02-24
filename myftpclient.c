#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	// "struct sockaddr_in"
#include <arpa/inet.h>	// "in_addr_t"
#include "myftp.h"

void list(int sd){
	struct message_s LIST_REQUEST; //to server
	struct message_s LIST_REPLY; //from server

	strcpy(LIST_REQUEST.protocol,"myftp");
	LIST_REQUEST.protocol[5]='\0';
	LIST_REQUEST.type = 0xA1;
	LIST_REQUEST.length = 10;
	char *buff;
	int len=0;

	if((len=sendn(sd,(void*)&LIST_REQUEST,sizeof(LIST_REQUEST)))<0){
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	printf("list request\n");

	buff = malloc(sizeof(char)*1024); //dynamic stack alloc later recv(len);->malloc(sizeof(char)*len)
	memset(buff, '\0', sizeof(char)*1024);

	if((len=recvn(sd,buff,sizeof(buff)))<0){
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
		exit(0);
	}
	printf("list recv\n");
	memcpy(&LIST_REPLY, buff, 10);

	printf("LIST_REPLY.protocol: %s vs myftp\n",LIST_REPLY.protocol);
	printf("LIST_REPLY.type: %x vs 0xA2\n",LIST_REPLY.type);
	printf("LIST_REPLY.length: %d vs %d\n",LIST_REPLY.length,len);
	if(strcmp(LIST_REPLY.protocol,"myftp") == 0 && LIST_REPLY.type == 0xA2 && LIST_REPLY.length == len){
		printf("%s",&buff[10]);
		free(buff);
	}
	else{
		perror("No list reply\n");
		exit(1);
	}
}

void main_task(in_addr_t ip, unsigned short port, char* op, char* filename)
{
	int buf;
	int fd;
	int choice;
	struct sockaddr_in addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){	// Create a TCP socket
		perror("socket()");
		exit(1);
	}

	// Below 4 lines: Set up the destination with IP address and port number.

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip;
	addr.sin_port = htons(port);

	if( connect(fd, (struct sockaddr *) &addr, addrlen) == -1 )		// connect to the destintation
	{
		perror("connect()");
		exit(1);
	}

	if(strcmp(op,"list")==0){
		list(fd);
	}
	else if(strcmp(op,"get")==0){

	}
	else if(strcmp(op,"put")==0){

	}
	else{
		perror("neither list, get or put can be performed");
		exit(1);
	}

	close(fd);	// Time to shut up
}

int main(int argc, char **argv)
{
	in_addr_t ip;
	unsigned short port;

	if((argc != 5 && argc != 4) || (argc == 5 && strcmp(argv[3],"list")==0) || (argc == 4 && strcmp(argv[3],"list")!=0))
	{
		fprintf(stderr, "Usage: %s [IP address] [port] [list|get|put] [filename]\n", argv[0]);
		exit(1);
	}

	if( (ip = inet_addr(argv[1])) == -1 )
	{
		perror("inet_addr()");
		exit(1);
	}

	port = atoi(argv[2]);
	if(argc == 4)
		main_task(ip, port, argv[3], NULL);
	else
		main_task(ip, port, argv[3], argv[4]);
	return 0;
}
