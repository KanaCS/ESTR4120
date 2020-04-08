#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h> // "struct sockaddr_in"
#include <arpa/inet.h>  // "in_addr_t"
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include "myftp.h"
#define DPATH "data/"

int block_size = 4096, n = 5, id=0, k = 2;

void list(int sd){
   int len=0, payload=0;//, *fd=(int*)sd;
   DIR *dir1;
   struct dirent *ptr;
   struct message_s LIST_REPLY;
   char *buff = malloc(sizeof(char)*1024);
   memset(buff, '\0', sizeof(char)*1024);
 
   if((dir1 = opendir(DPATH))==NULL){
	   perror("dir doesnt exist");
	   exit(0);
   }
   printf("====================\n");
   while((ptr = readdir(dir1)) != NULL){
   	printf("d_name: %s\n",ptr->d_name);
	if(strcmp(ptr->d_name, "metadata")!=0){
   		strcpy(&buff[10 + payload], ptr->d_name);
   		payload += strlen(ptr->d_name)+1;
   		buff[10 + payload - 1] = '\n';
	}
   }
   printf("====================\n");
   closedir(dir1);
 
   memcpy(&LIST_REPLY.protocol,"myftp",5);
   LIST_REPLY.type = 0xA2;
   LIST_REPLY.length = ntohl(10 + payload);
   memcpy(buff, &LIST_REPLY, 10);
   //printf("before:[%c %c %c]\n",buff[10],buff[11],buff[12]);
   if((len=sendn(sd,buff,10+payload))<0){
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
   	GET_REPLY.length = ntohl(10);
 
   	char *buff = malloc(sizeof(char) * 10);
   	memcpy(buff, &GET_REPLY, 10);
   	int len = 0;
   	if((len=sendn(sd,(void*)buff,10))<0){
       	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
       	exit(0);
   	}
   	free(buff);
   }
   else { // file found, send GET_REPLY and send file
   	// GET_REPLY
   	int len = 0, num_str_len = 0;
	char meta_file_path[100];
	strcpy(meta_file_path, "./metadata/");
	strcpy(meta_file_path + 11, file_name);
	FILE *meta_fp = fopen(meta_file_path, "r"); 
	char *buff = malloc(sizeof(char) * 50);
	char num_str[50],c;
	// put id into buff[10]
	
	num_str_len = sprintf(num_str, "%d ", id);
	strcpy(buff + 10, num_str);

	// put filesize into buff[10+sth]
	int i = 0;
	while((c = fgetc(meta_fp)) != EOF) {
		num_str[i] = c;
	}
	num_str[i] = '\0';
	strcpy(buff + 10 + num_str_len, num_str);

	struct message_s GET_REPLY; memcpy(GET_REPLY.protocol,"myftp", 5); GET_REPLY.type = 0xB2; GET_REPLY.length = ntohl(10+num_str_len+i);
   	memcpy(buff, &GET_REPLY, 10);

   	if( (len = sendn(sd, (void *)buff, 10+num_str_len+i)) < 0) {
       	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
       	exit(0);
   	}
   	free(buff);
   	// FILE_DATA
 
   	char *file_path = malloc(sizeof(char) *(strlen(DPATH) + strlen(file_name)));
   	strcpy(file_path, DPATH);
   	strcpy(&file_path[strlen(DPATH)], file_name);
   	FILE *fd = fopen(file_path, "r");
  
  
       	if( (len = sendn(sd, (void *)buff, s+10)) < 0) {
           	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
           	exit(0);
       	}
   	}
   	FILE_DATA.length = ntohl(10);
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
 
void put(int sd, char *file_name){
	int len=0;
	unsigned int file_data_len;
	struct message_s PUT_REPLY;
	struct message_s FILE_DATA;
	char *full_path = malloc(sizeof(char) * 50);
	memcpy(full_path, DPATH, DPATH_LEN);
	strcpy(&full_path[DPATH_LEN], file_name);

	FILE *fp = fopen(full_path, "w");
	if(fp == NULL) {
		perror("Write file error\n"); exit(1);
	}
	char *buff = (char*)malloc(sizeof(char)*(BATCH_SIZE+10));
	// memset(buff, '\0', sizeof(char)*(BATCH_SIZE+10));

	memcpy(PUT_REPLY.protocol,"myftp",5);
	PUT_REPLY.type = 0xC2;
	PUT_REPLY.length = ntohl(10);
	memcpy(buff, &PUT_REPLY, 10);
	if((len=sendn(sd,(void*)buff, 10))<0){
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}

	unsigned long long dl = 0;
	char *message_str = malloc(sizeof(char) *50); 
	while(1) {
		if( (len=recvn(sd, (void *)buff, 10) ) < 0 ) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
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
		// printf("RECEIVING %d BYTES BATCH\n", file_data_len);
		if(file_data_len == 0) {
			//recv metadata
			if( (len=recvn(sd, (void *)buff, 20) ) < 0 ) {
				printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
			}
			//store in metafile
			char path[100]; memset(path, '\0', sizeof(path));
			mkdir("data/metadata", 0700);
			strcpy(path, "data/metadata/");
			strcat(path, file_name);
			FILE *pt = fopen(path, "w+");
			fprintf(pt,"%s\n",buff);
			fclose(pt);
			break;
		}
		if( (len=recvn(sd, (void *)buff, file_data_len) ) < 0 ) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
		}

		dl += fwrite(buff, 1, len, fp);
		showBytes("Received ", message_str, dl);
		printf("\r%s", message_str);
	}
	printf("\n");
	free(message_str);
	free(buff);
	fclose(fp);
}

void *option(void *sd){
 
   int len=0, *fd;
 
   char buff[11];
   char *pl_buff;
   struct message_s REQUEST;
   fd=(int*)sd;
   //printf("in option\n");
   //printf("fd = %d\n",*fd);
   if((len=recvn(*fd,buff, 10))<0){
   		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
   }

   memcpy(&REQUEST, buff, 10);
   REQUEST.length = ntohl(REQUEST.length);
   //printf("REQUEST recved: %d\n",REQUEST.length);
   //printf("REQUEST.protocol:%s   %d\n",REQUEST.protocol,memcmp(&REQUEST.protocol,"myftp",5) == 0);
    //printf("REQUEST.type: %x\n",REQUEST.type);
    //printf("REQUEST.length:%d %d\n",REQUEST.length,len);
   if(REQUEST.length > 10) {
   		pl_buff = malloc(sizeof(char) * (REQUEST.length-10));
		if((len=recvn(*(int*)sd, pl_buff, REQUEST.length-10))<0){
			printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
   		}
   }
   //printf("outside\n");
   //printf("\nbuff: %s\n\n",buff);

   if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xA1){ //list
   		list(*fd);
   }
 
   else if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xB1){//get
   		get(*(int*)sd, pl_buff);
   }
   else if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xC1){//put
	if(REQUEST.length != 0)
   		put(*fd, pl_buff);
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
   int fd, i=0;
   struct sockaddr_in addr;
   unsigned int addrlen = sizeof(struct sockaddr_in);
   pthread_t thread[15];
   int accept_fd[15];
   struct sockaddr_in tmp_addr[15];
 
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
   i = 0;
   while(1){
        if(i == 14) i=0;
        if( (accept_fd[i] = accept(fd, (struct sockaddr*) &tmp_addr[i], &addrlen)) == -1 ){
            perror("accept()");
            continue;
        }
        printf("accept_fd[%d] = %d\n",i, accept_fd[i]);
        if(pthread_create(&thread[i], NULL, option, &accept_fd[i])!=0){
            perror("pthread error");
            exit(1);
        }
        i++;
   }
   for (i = 0; i < 15; i++)
    pthread_join(thread[i], NULL);
}
 
int main(int argc, char **argv)
{
   unsigned short port;
   char tmp[20];
   	
   if(argc != 2 || strcmp(argv[1],"serverconfig.txt")!=0){
   	fprintf(stderr, "Usage: %s serverconfig.txt\n", argv[0]);
   	exit(1);
   }
	
   FILE *fp = fopen(argv[1], "r");
   if(fp==NULL){
	perror("serverconfig.txt is not found");
	exit(1);
   }

    fscanf(fp, "%[^\n]\n", tmp); n = atoll(tmp); 
    fscanf(fp, "%[^\n]\n", tmp); k = atoll(tmp);
    fscanf(fp, "%[^\n]\n", tmp); id = atoll(tmp); 
    fscanf(fp, "%[^\n]\n", tmp); block_size = atoll(tmp); 
    fscanf(fp, "%[^\n]\n", tmp); port = atoi(tmp); 
    //printf("n:%d, k:%d, bs:%llu\n",n,k,block_size);
 
   main_loop(port);
 
   return 0;
}
 
 
