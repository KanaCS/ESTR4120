#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	// "struct sockaddr_in"
#include <arpa/inet.h>	// "in_addr_t"
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include "myftp.h"

#define DPATH "data/"
void list(int sd){
	int len=0, payload=1;//, *fd=(int*)sd;
	DIR *dir1;
	struct dirent *ptr;
	struct message_s LIST_REPLY; 
	char *buff = malloc(sizeof(char)*1024);
	memset(buff, '\0', sizeof(char)*1024);


	dir1 = opendir(DPATH);
	printf("====================\n");
	while((ptr = readdir(dir1)) != NULL){
		printf("d_name: %s\n",ptr->d_name);
		strcpy(&buff[10 + payload], ptr->d_name);
		payload += strlen(ptr->d_name)+1;
		buff[10 + payload - 1] = '\n';
	}
	printf("====================\n");
	closedir(dir1);

	strcpy(LIST_REPLY.protocol,"myftp");
	LIST_REPLY.type = 0xA2;
	LIST_REPLY.length = 10;
	memcpy(buff, &LIST_REPLY, 10);

	if((len=sendn(sd,(void*)buff,sizeof(char)*1024))<0){
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	close(sd);
}

void get(int sd, char *file_name) {
	DIR *dir1;
	dir1 = opendir(DPATH);
	struct dirent *ptr_dirent;
	int file_found = 0;
	while((ptr_dirent= readdir(dir1)) != NULL) {
		if(strcmp(ptr_dirent->d_name, file_name) == 0) {
			file_found = 1;
			break;
		}
	}
	if(file_found == 0) { // file not found
		// GET_REPLY
		struct message_s GET_REPLY;
		strcpy(GET_REPLY.protocol,"myftp");
		GET_REPLY.type = 0xB3;
		GET_REPLY.length = 10;

		char *buff = malloc(sizeof(char) * 10);
		memcpy(buff, &GET_REPLY, 10);
		int len = 0;
		if((len=sendn(sd,(void*)buff,sizeof(buff)))<0){
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		free(buff);
	}
	else { // file found, send GET_REPLY and send file
		// GET_REPLY
		int len = 0;
		struct message_s GET_REPLY; memcpy(GET_REPLY.protocol,"myftp", 5); GET_REPLY.type = 0xB2; GET_REPLY.length = 10;
		char *buff = malloc(sizeof(char) * 10); 
		memcpy(buff, &GET_REPLY, 10);
		if( (len = sendn(sd, (void *)buff, 10)) < 0) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		free(buff);
		// FILE_DATA

		char *file_path = malloc(sizeof(char) *(strlen(DPATH) + strlen(file_name)));
		strcpy(file_path, DPATH);
		strcpy(&file_path[strlen(DPATH)], file_name);
		FILE *fd = fopen(file_path, "r");

		fseek(fd, 0, SEEK_END);
		unsigned long long file_len = ftell(fd);
		fseek(fd, 0, SEEK_SET);

		int s = 0;
		buff = malloc(sizeof(char)* (BATCH_SIZE + 10));
		unsigned long long req_batch = file_len / BATCH_SIZE + 1, i = 0;
		struct message_s FILE_DATA; strcpy(FILE_DATA.protocol,"myftp"); FILE_DATA.type = 0xFF; 
		for(i = 0; i < req_batch; i++) {
			s = fread(&buff[10], 1, BATCH_SIZE, fd);
			FILE_DATA.length = s+10;
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
		free(file_path);
		fclose(fd);
		free(buff);
	}
}

void put(int sd){
	int len=0;
	struct message_s PUT_REPLY; 
	struct message_s FILE_DATA; 
	char *buff = (char*)malloc(sizeof(char)*1024);
	memset(buff, '\0', sizeof(char)*1024);

	strcpy(PUT_REPLY.protocol,"myftp");
	PUT_REPLY.type = 0xC2;
	PUT_REPLY.length = 10;
	memcpy(buff, &PUT_REPLY, 10);

	if((len=sendn(sd,(void*)&PUT_REPLY,sizeof(PUT_REPLY)))<0){
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}

	if((len=recvn(sd,buff,sizeof(buff)))<0){
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
		exit(0);
	}
	memcpy(&FILE_DATA, buff, 10);
	if(FILE_DATA.protocol[0]!='m' || FILE_DATA.protocol[1]!='y'|| FILE_DATA.protocol[2]!='f' && FILE_DATA.protocol[3]!='t' && FILE_DATA.protocol[4]!='p' || FILE_DATA.type != 0xFF || FILE_DATA.length != len){
		perror("file data put server failure\n");
		exit(1);	
	}

	//unfinished
}


void *option(void *sd){

	int len=0, *fd;

	char buff[11];
	char *pl_buff;
	struct message_s REQUEST;
	fd=(int*)sd;
	//printf("fd = %d\n",*fd);
	if((len=recvn(*(int*)sd,buff, 10))<0){
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
	}
	memcpy(&REQUEST, buff, 10);
	if(REQUEST.length > 10) {
		pl_buff = malloc(sizeof(char) * (REQUEST.length-10));
	}
	if((len=recvn(*(int*)sd, pl_buff, REQUEST.length-10))<0){
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
	}
	//printf("\nbuff: %s\n\n",buff);

	// printf("heyyyyy???\n");
	// printf("REQUEST.protocol:%s   %d\n",REQUEST.protocol,strcmp(REQUEST.protocol,"myftp") == 0);
	// printf("REQUEST.type: %d\n",REQUEST.type==0xA1);
	// printf("REQUEST.length:%d\n",REQUEST.length==len);
	if(REQUEST.protocol[0]!='m' || REQUEST.protocol[1]!='y'|| REQUEST.protocol[2]!='f' && REQUEST.protocol[3]!='t' && REQUEST.protocol[4]!='p'){
		perror("server request failure\n");
		exit(1);	
	}
	if(REQUEST.type == 0xA1 && REQUEST.length == len){ //list
		list(*fd);
	}
	else if(REQUEST.type == 0xB1){//get
		get(*(int*)sd, pl_buff);
	}
	else if(REQUEST.type == 0xC1 && REQUEST.length == len){//put
		put(*fd);
	}
	else{
		perror("server request failure\n");
		exit(1);
	}
	free(pl_buff);
	pthread_exit(NULL);

}
	



void main_loop(unsigned short port)
{
	int fd, accept_fd, client_count;
	struct sockaddr_in addr, tmp_addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	pthread_t thread;

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ // Create a TCP Socket
		perror("socket()");
		exit(1);
	}

	long val = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1){
		perror("setsockopt");
		exit(1);
	}
	// 4 lines below: setting up the port for the listening socket

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	// After the setup has been done, invoke bind()

	if(bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1){
		perror("bind()");
		exit(1);
	}

	// Switch to listen mode by invoking listen()

	if( listen(fd, 1024) == -1 ){
		perror("listen()");
		exit(1);
	}

	printf("[To stop the server: press Ctrl + C]\n");

	client_count = 0;
	while(1) {
		// Accept one client
		if( (accept_fd = accept(fd, (struct sockaddr *) &tmp_addr, &addrlen)) == -1){
			perror("accept()");
			exit(1);
		}
		printf("accept_fd = %d\n",accept_fd);
		client_count++;

		if (pthread_create(&thread, NULL, option, &accept_fd)) { 
			perror("pthread error");
			exit(1);
		}

		//close(fd);	// Time to shut up.
	}	// End of infinite, accepting loop.
}

int main(int argc, char **argv)
{
	unsigned short port;

	if(argc != 2){
		fprintf(stderr, "Usage: %s [port]\n", argv[0]);
		exit(1);
	}

	port = atoi(argv[1]);

	main_loop(port);

	return 0;
}
