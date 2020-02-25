#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // "struct sockaddr_in"
#include <arpa/inet.h>  // "in_addr_t"
#include "myftp.h"
 
void list(int sd){
   struct message_s LIST_REQUEST; //to server
   struct message_s LIST_REPLY; //from server
   //strcpy(LIST_REQUEST.protocol,"myftp");
   memcpy(&LIST_REQUEST,"myftp",5);
   LIST_REQUEST.type = 0xA1;
   LIST_REQUEST.length = 10;
   char *buff;
   int len=0;
 
   if((len=sendn(sd,(void*)&LIST_REQUEST,sizeof(LIST_REQUEST)))<0){ //send LIST_REQUEST
   	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
   	exit(0);
   }
   //printf("list request\n");
 
   buff = malloc(sizeof(char)*1024); //a block size of 1024, transmit data per block of 1024
   memset(buff, '\0', sizeof(buff));
 
   if((len=recvn(sd,buff,sizeof(char)*1024))<0){ //recv LIST_REPLY
   	printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
   	exit(0);
   }
   //printf("[%c %c %c]\n",buff[10],buff[11],buff[12]);
   printf("list recv\n");
   memcpy(&LIST_REPLY, buff, 10);
   //printf("buff: %s\n\n",buff);
   //printf("LIST_REPLY.protocol: %s vs myftp\n",LIST_REPLY.protocol);
   //printf("LIST_REPLY.type: %x vs 0xA2\n",LIST_REPLY.type);
   //printf("LIST_REPLY.length: %d vs %d\n",LIST_REPLY.length,len);
 
   if(memcmp(LIST_REPLY.protocol,"myftp",5) == 0 && LIST_REPLY.type == 0xA2){
   	printf("%s",&buff[10]); // ===========list dir=============
   	free(buff);
   }
   else{
   	perror("No list reply\n");
   	exit(1);
   }
}
 
void put(int sd, char *filename){
	struct message_s PUT_REQUEST; //to server
	struct message_s PUT_REPLY; //from server
	struct message_s FILE_DATA; //to server
	
	char *file_path = malloc(sizeof(char) *(strlen(DPATH) + strlen(filename)));
   	strcpy(file_path, DPATH);
   	strcpy(&file_path[strlen(DPATH)], filename);
	FILE *fp = fopen(file_path, "r");
	if(fp==NULL){
		perror("requested upload file doesn't exist");
		exit(1);
	}

	// PUT_REQUEST
	strcpy(PUT_REQUEST.protocol, PROTOCOL_CODE);
	PUT_REQUEST.type = 0xC1;
	int header_len = 10 + strlen(filename) + 1;
	PUT_REQUEST.length = header_len;
	char *buff = malloc(sizeof(char) *(header_len));

	memcpy(buff, &PUT_REQUEST, header_len);
	strcpy(&buff[10], filename);
	unsigned int len=0;

	if((len=sendn(sd, (void*)buff, header_len))<0){ //send PUT_REQUEST
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}

	// PUT_REPLY
	if((len=recvn(sd, buff, 10))<0){ //recv PUT_REPLY
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
		exit(0);
	}
	memcpy(&PUT_REPLY, buff, 10);
	printf("PUT_REPLY.protocol:%s\n",PUT_REPLY.protocol);
	printf("PUT_REPLY.type:%x\n",PUT_REPLY.type);
	printf("PUT_REPLY.len:%d\n",PUT_REPLY.length);
	
	if(memcmp(PUT_REPLY.protocol, PROTOCOL_CODE, 5) != 0) {
		perror("Wrong protocol code in PUT_REPLY header\n"); exit(1);
	}
	if(PUT_REPLY.type != 0xC2) { 
		perror("Wrong type code in PUT_REPLY header\n"); exit(1);
	}
	free(buff);

	// FILE_DATA
	fseek(fp, 0, SEEK_END);
	unsigned long long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned long long req_batch = size / BATCH_SIZE + 1 , i = 0;
	unsigned int s = 0;
	buff = malloc(sizeof(char) * (BATCH_SIZE + 10));
	strcpy(FILE_DATA.protocol, PROTOCOL_CODE); FILE_DATA.type = 0xFF;
	for(i = 0; i < req_batch; i++) {
		s = fread(buff, 1, BATCH_SIZE, fp);
		FILE_DATA.length = s + 10;
		memcpy(buff, &FILE_DATA, 10);
		if( (len = sendn(sd, (void *)buff, s+10)) < 0) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
	}
	FILE_DATA.length = 10;
	memcpy(buff, &FILE_DATA, 10);
	if( (len = sendn(sd, (void *)buff, 10)) < 0) {
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	free(buff);
	fclose(fp);
}
 
void get(int sd, char* file_name) {
	struct message_s GET_REQUEST; //to server
	int file_name_len = strlen(file_name);

	strcpy(GET_REQUEST.protocol, PROTOCOL_CODE);
	GET_REQUEST.type = 0xB1;
	GET_REQUEST.length = 10 + file_name_len + 1;
	char *buff = malloc(sizeof(char)*(10 + file_name_len + 1));
	int len=0;

	memcpy(buff, &GET_REQUEST, 10);
	memcpy(&buff[10], (void *)file_name, file_name_len);
	buff[10 + file_name_len] = '\0';
	if( (len=sendn(sd, (void *)buff, 10 + file_name_len + 1) ) < 0 ) {
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
	}
	free(buff);

	struct message_s GET_REPLY; //from server
	buff = malloc(sizeof(char) * 10);
	if( (len=recvn(sd, (void *)buff, 10) ) < 0 ) {
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
	}
	memcpy(&GET_REPLY, buff, 10);
	// if(len != GET_REPLY.length) {
	// 	printf("Recv Error: expect %d, recv %d\n", GET_REPLY.length, len); exit(0);
	// }
	
	free(buff);
	if(memcmp(GET_REPLY.protocol, PROTOCOL_CODE, 5) != 0) {
		perror("Wrong protocol code in GET_REPLY\n"); exit(1);
	}
	if(GET_REPLY.type == 0xB2) {
		struct message_s FILE_DATA;
		buff = malloc(sizeof(char) * BATCH_SIZE);
		unsigned int file_data_len = 0;
		char *file_path = malloc(sizeof(char) * (DPATH_LEN + file_name_len));
		memcpy(file_path, DPATH, DPATH_LEN);
		memcpy(&file_path[DPATH_LEN], file_name, file_name_len);
		FILE *fp = fopen(file_path, "w");
		unsigned long long dl = 0;
		char *showMessage = malloc(sizeof(char) *50);
		while(1) {
			if( (len=recvn(sd, (void *)buff, 10) ) < 0 ) {
				printf("Receive file Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
			}
			memcpy(&FILE_DATA, buff, 10);
			if(memcmp(FILE_DATA.protocol, PROTOCOL_CODE, 5) != 0) {
				perror("Wrong protocol code in FILE_DATA header\n"); exit(1);
			}
			if(FILE_DATA.type != 0xFF) {
				perror("Wrong type code in FILE_DATA header\n"); exit(1);
			}
			file_data_len = FILE_DATA.length - 10;
			if(file_data_len == 0) {
				break;
			}
			if( (len=recvn(sd, (void *)buff, file_data_len) ) < 0 ) {
				printf("Receive file Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
			}
			dl += fwrite(buff, 1, len, fp);
			showLoaderBytes("Downloaded ", showMessage, dl);
			printf("\r%s", showMessage);
		}
		printf("\n");
		fclose(fp);
		free(buff);
		free(file_path);
	}
	else {
		perror("File not found\n");
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
 
   if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){   // Create a TCP socket
   	perror("socket()");
   	exit(1);
   }
   printf("socket created");
 
   // Below 4 lines: Set up the destination with IP address and port number.
 
   memset(&addr, 0, sizeof(struct sockaddr_in));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = ip;
   addr.sin_port = htons(port);
 
	// printf("%s len:%d\n", filename, strlen(filename));
   if( connect(fd, (struct sockaddr *) &addr, addrlen) == -1 ) 	// connect to the destintation
   {
   	perror("connect()");
   	exit(1);
   }
 
   if(strcmp(op,"list")==0){
   		list(fd);
		printf("going into list");
   }
   else if(strcmp(op,"get")==0){
   		get(fd, filename);
		printf("going into get");
   }
   else if(strcmp(op,"put")==0){
   		put(fd, filename);
		printf("going into put");
   }
   else{
   	perror("neither list, get or put can be performed");
   	exit(1);
   }
 
   close(fd);  // Time to shut up
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
 
 
 
 

