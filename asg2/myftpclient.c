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
   LIST_REQUEST.length = ntohl(10);
   char *buff;
   int len=0;
   //printf("list sending\n");
   if((len=sendn(sd,(void*)&LIST_REQUEST,10))<0){ //send LIST_REQUEST
   	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
   	exit(0);
   }
   //printf("list request\n");
 
   buff = malloc(sizeof(char)*BATCH_SIZE); //a block size of 1024, transmit data per block of 1024
   memset(buff, '\0', sizeof(buff));
   //printf("list waiting recv\n"); 
   if((len=recvn(sd,buff, 10))<0){ //recv LIST_REPLY
   	printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
   	exit(0);
   }
  //printf("[%c %c %c]\n",buff[10],buff[11],buff[12]);
   //printf("list recv\n");
   memcpy(&LIST_REPLY, buff, 10);
   LIST_REPLY.length = ntohl(LIST_REPLY.length);
   //printf("buff: %s\n\n",buff);
   //printf("LIST_REPLY.protocol: %s vs myftp\n",LIST_REPLY.protocol);
   //printf("LIST_REPLY.type: %x vs 0xA2\n",LIST_REPLY.type);
   //printf("LIST_REPLY.length: %d vs %d\n",LIST_REPLY.length,len);
 
   if(memcmp(LIST_REPLY.protocol,"myftp",5) == 0 && LIST_REPLY.type == 0xA2){
	unsigned int pl_size = LIST_REPLY.length - 10;
	if((len=recvn(sd, buff, pl_size))<0){ 
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
		exit(0);
	}
	printf("=========list dir===========\n");
   	printf("%s",buff); 
	printf("============================\n");
   	free(buff);
   }
   else{
	printf("LIST_REPLY.type= %02x\n", LIST_REPLY.type);
   	perror("No correct list reply\n");
   	exit(1);
   }
}
 
void put(int sd, char *filename){
	struct message_s PUT_REQUEST; //to server
	struct message_s PUT_REPLY; //from server
	struct message_s FILE_DATA; //to server
	
	int notfound = 0;
	FILE *fp = fopen(filename, "r");
	if(fp==NULL){
			perror("requested upload file doesn't exist");
			notfound = 1; //exit(1);
	}

	//printf("put request\n");
	// PUT_REQUEST
	strcpy(PUT_REQUEST.protocol, PROTOCOL_CODE);
	PUT_REQUEST.type = 0xC1;
	int header_len = 10 + strlen(filename) + 1;
	if(notfound == 1) PUT_REQUEST.length = ntohl(0);
	else PUT_REQUEST.length = ntohl(header_len);
	char *buff = malloc(sizeof(char) *(header_len));

	memcpy(buff, &PUT_REQUEST, header_len);
	strcpy(&buff[10], filename);
	unsigned int len=0;
	//printf("sendn\n");
	if((len=sendn(sd, (void*)buff, header_len))<0){ //send PUT_REQUEST
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	
	if(notfound == 1){
			free(buff);
			exit(0);
	}
	//printf("put reply\n");
	// PUT_REPLY
	if((len=recvn(sd, buff, 10))<0){ //recv PUT_REPLY
		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
		exit(0);
	}
	memcpy(&PUT_REPLY, buff, 10);
	   PUT_REPLY.length = ntohl(PUT_REPLY.length);
	//printf("PUT_REPLY.protocol:%s\n",PUT_REPLY.protocol);
	//printf("PUT_REPLY.type:%x\n",PUT_REPLY.type);
	//printf("PUT_REPLY.len:%d\n",PUT_REPLY.length);
	
	if(memcmp(PUT_REPLY.protocol, PROTOCOL_CODE, 5) != 0) {
		perror("Wrong protocol code in PUT_REPLY header\n"); exit(1);
	}
	if(PUT_REPLY.type != 0xC2) { 
		perror("Wrong type code in PUT_REPLY header\n"); exit(1);
	}
	free(buff);
	//printf("file data handle\n");
	// FILE_DATA
	fseek(fp, 0, SEEK_END);
	unsigned long long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned long long req_batch = size / BATCH_SIZE + 1 , i = 0;
	unsigned int s = 0;
	buff = malloc(sizeof(char) * (BATCH_SIZE + 10));
	strcpy(FILE_DATA.protocol, PROTOCOL_CODE); FILE_DATA.type = 0xFF;
	for(i = 0; i < req_batch; i++) {
		s = fread(&buff[10], 1, BATCH_SIZE, fp);
		FILE_DATA.length = ntohl(s + 10);
		memcpy(buff, &FILE_DATA, 10);
		if( (len = sendn(sd, (void *)buff, s+10)) < 0) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
	}
	FILE_DATA.length = ntohl(10);
	memcpy(buff, &FILE_DATA, 10);
	//printf("sendn file\n");
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
	GET_REQUEST.length = ntohl(10 + file_name_len + 1);
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
	GET_REPLY.length = ntohl(GET_REPLY.length);
	// if(GET_REPLY.length == 0){
	// 	perror("requested upload file doesn't exist: No such file or directory\n"); exit(1);
	// }
	if(len != GET_REPLY.length) {
		printf("Recv Error: expect %d, recv %d\n", GET_REPLY.length, len); exit(0);
	}
	
	free(buff);
	if(memcmp(GET_REPLY.protocol, PROTOCOL_CODE, 5) != 0) {
		perror("Wrong protocol code in GET_REPLY\n"); exit(1);
	}
	if(GET_REPLY.type == 0xB2) {
		struct message_s FILE_DATA;
		buff = malloc(sizeof(char) * BATCH_SIZE);
		unsigned int file_data_len = 0;
		char *file_path = malloc(sizeof(char) * (DPATH_LEN + file_name_len));
		// memcpy(file_path, DPATH, DPATH_LEN);
		strcpy(file_path, file_name);
		FILE *fp = fopen(file_name, "w");
		unsigned long long dl = 0;
		char *showMessage = malloc(sizeof(char) *50);
		while(1) {
			if( (len=recvn(sd, (void *)buff, 10) ) < 0 ) {
				printf("Receive file Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
			}
			memcpy(&FILE_DATA, buff, 10);
			FILE_DATA.length = ntohl(FILE_DATA.length);
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
 
 
void main_task(in_addr_t* ip, unsigned short* port, char* op, char* filename, int server_num, int n, int k)
{
	int buf;
	int fd[server_num];
	int choice;
	int count = server_num;
	struct sockaddr_in addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	fd_set fds;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	//set up connection to other servers
	int i = 0, found = 1;
	for(i=0; i<server_num; i++){
		found = 1;
		if((fd[i] = socket(AF_INET, SOCK_STREAM, 0)) == -1){   // Create a TCP socket
			perror("socket()");
			found = 0;  count --;
		}
		//printf("socket created");
		
		// Below 4 lines: Set up the destination with IP address and port number.
		printf("port: %d\n",port[i]);		
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = ip[i];
		addr.sin_port = htons(port[i]);
		
			// printf("%s len:%d\n", filename, strlen(filename));
		if( connect(fd[i], (struct sockaddr *) &addr, addrlen) == -1 ) 	// connect to the destintation
		{
			perror("connect()");
			if(found!=0) count--;
		}

		if(strcmp(op,"list")==0){
			if(found == 1){
				list(fd[i]); 
				break;
			}
		}
		else if(strcmp(op,"put")==0){
			if(found == 0){
				perror("not all server available for \"put\"");
				exit(0);
			}
		}
		else if(strcmp(op,"get")==0){
			if(count<k){
				perror("less than k servers available for \"get\"");
				exit(0);
			}
		}
		else{
			perror("neither list, get or put can be performed");
			exit(1);
		}
	}
	
	//############### PUT & GET ################	
	//for (;;) {
		// we now select the descriptors peerSd and STDIN
		FD_ZERO(&fds);
		int max = fd[0];
		for (i=0; i<server_num ; i++){
			FD_SET(fd[i], &fds);
			if (fd[i]>max) max = fd[i];
		}
		printf("fd seted\n");
		select(max + 1, NULL, &fds, NULL, &tv);
		printf("selected\n");

		// check if each socket descriptor is set
		for (i=0; i<server_num ; i++){
			if(FD_ISSET(fd[i],&fds)){
				if(strcmp(op,"put")==0){
					printf("to put\n");
					put(fd[i], filename);
				}
				else if(strcmp(op,"get")==0){
					printf("to get\n");
					get(fd[i], filename);
				}
			}
		}
	//}

	for (i=0; i<server_num ; i++){
		close(fd[i]);  // Time to shut up
	}
}
 
int main(int argc, char **argv)
{
	unsigned long long block_size=0;
    int n=0, k=0, lines=0;
    in_addr_t *ip;
    unsigned short *port;
    char tmp[20], tmp_ip[20], tmp_port[20];

   	if((argc != 4 && argc != 3) || (argc == 4 && strcmp(argv[2],"list")==0) || (argc == 3 && strcmp(argv[2],"list")!=0))
   	{
   		fprintf(stderr, "Usage: %s clientconfig.txt <list|get|put> <file>\n", argv[0]);
   		exit(1);
   	}

   	FILE *fp = fopen(argv[1], "r");
	if(fp==NULL){
			perror("clientconfig.txt is not found");
			exit(1);
	}

    while( EOF != fscanf(fp, "%[^\n]\n", tmp) ){
        lines += 1;
    }

    if(lines-3 <= 0){
        perror("no available server is found");
        exit(1);
    }
    ip = (in_addr_t*)malloc(sizeof(in_addr_t)*(lines-3)); port = (unsigned short*)malloc(sizeof(unsigned short)*(lines-3));
	fseek(fp, 0, SEEK_SET);

    fscanf(fp, "%[^\n]\n", tmp); n = atoll(tmp); 
    fscanf(fp, "%[^\n]\n", tmp); k = atoll(tmp);
    fscanf(fp, "%[^\n]\n", tmp); block_size = atoll(tmp); 
    //printf("n:%d, k:%d, bs:%llu\n",n,k,block_size);

    int i = 0;
    while (EOF != fscanf(fp, "%[^:]", tmp_ip) && fread(tmp, 1, 1, fp)!=0 && EOF != fscanf(fp, "%[^\n]\n", tmp_port))
    {
        if ((ip[i] = inet_addr(tmp_ip)) == -1){
			perror("inet_addr()");
			exit(1);
		}
        port[i] = atoi(tmp_port); 
        printf(">%s %d\n",tmp_ip, port[i]);
	i++;
    }
 
    if(argc == 3)
   		main_task(ip, port, argv[2], NULL, lines-3, n, k);
    else
   		main_task(ip, port, argv[2], argv[3], lines-3, n, k);
	    
    fclose(fp);
    free(ip);
    free(port);
    return 0;
}
  
