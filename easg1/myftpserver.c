#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // "struct sockaddr_in"
#include <arpa/inet.h>  // "in_addr_t"
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "myftp.h"

#define DPATH "data/"

#define RSA_SERVER_CERT     "./server_cert_and_key/server_cert.pem"
#define RSA_SERVER_KEY      "./server_cert_and_key/server_key.pem"

SSL_CTX *sslctx;

SSL_CTX * sslctx_server() {
	SSL_CTX         *ctx;
	SSL_METHOD      *meth;
	X509            *client_cert = NULL;

	
	/*----------------------------------------------------------------*/
	/* Register all algorithms */
	OpenSSL_add_all_algorithms();

	/* Load encryption & hashing algorithms for the SSL program */
	SSL_library_init();

	/* Load the error strings for SSL & CRYPTO APIs */
	SSL_load_error_strings();

	/* Create a SSL_METHOD structure (choose a SSL/TLS protocol version) */
	meth = (SSL_METHOD*)TLSv1_method();

	/* Create a SSL_CTX structure */
	ctx = SSL_CTX_new(meth);

	if (!ctx) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	/* Load the server certificate into the SSL_CTX structure */
	if (SSL_CTX_use_certificate_file(ctx, RSA_SERVER_CERT, SSL_FILETYPE_PEM)
			<= 0) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	/* set password for the private key file. Use this statement carefully */
	SSL_CTX_set_default_passwd_cb_userdata(ctx, (char*)"4430");

	/* Load the private-key corresponding to the server certificate */
	if (SSL_CTX_use_PrivateKey_file(ctx, RSA_SERVER_KEY, SSL_FILETYPE_PEM) <=
			0) { 
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	/* Check if the server certificate and private-key matches */
	if (!SSL_CTX_check_private_key(ctx)) {
		fprintf(stderr,
				"Private key does not match the certificate public key\n");
		exit(1);
	}

    return ctx;
}


void list(SSL *ssl){
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
   	strcpy(&buff[10 + payload], ptr->d_name);
   	payload += strlen(ptr->d_name)+1;
   	buff[10 + payload - 1] = '\n';
   }
   printf("====================\n");
   closedir(dir1);
 
   memcpy(&LIST_REPLY.protocol,"myftp",5);
   LIST_REPLY.type = 0xA2;
   LIST_REPLY.length = ntohl(10 + payload);
   memcpy(buff, &LIST_REPLY, 10);
   //printf("before:[%c %c %c]\n",buff[10],buff[11],buff[12]);
   if((len=sendn(ssl,buff,10+payload))<0){
   	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
   	exit(0);
   }
}
 
void get(SSL *ssl, char *file_name) {
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
   	if((len=sendn(ssl,(void*)buff,10))<0){
       	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
       	exit(0);
   	}
   	free(buff);
   }
   else { // file found, send GET_REPLY and send file
   	// GET_REPLY
   	int len = 0;
   	struct message_s GET_REPLY; memcpy(GET_REPLY.protocol,"myftp", 5); GET_REPLY.type = 0xB2; GET_REPLY.length = ntohl(10);
   	char *buff = malloc(sizeof(char) * 10);
   	memcpy(buff, &GET_REPLY, 10);
   	if( (len = sendn(ssl, (void *)buff, 10)) < 0) {
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
       	FILE_DATA.length = ntohl(s+10);
       	memcpy(buff, &FILE_DATA, 10);
       	if( (len = sendn(ssl, (void *)buff, s+10)) < 0) {
           	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
           	exit(0);
       	}
   	}
   	FILE_DATA.length = ntohl(10);
   	memcpy(buff, &FILE_DATA, 10);
   	if( (len = sendn(ssl, (void *)buff, 10)) < 0) {
       	printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
       	exit(0);
   	}
   	free(file_path);
   	fclose(fd);
   	free(buff);
   }
}
 
void put(SSL *ssl, char *file_name){
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

	if((len=sendn(ssl,(void*)buff, 10))<0){
		printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}

	unsigned long long dl = 0;
	char *message_str = malloc(sizeof(char) *50); 
	while(1) {
		if( (len=recvn(ssl, (void *)buff, 10) ) < 0 ) {
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
			break;
		}
		if( (len=recvn(ssl, (void *)buff, file_data_len) ) < 0 ) {
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno); exit(0);
		}

		dl += fwrite(buff, 1, len, fp);
		showLoaderBytes("Received ", message_str, dl);
		printf("\r%s", message_str);
	}
	printf("\n");
	free(message_str);
	free(buff);
	fclose(fp);
}
 
 
void *option(void *sd){
 
   int len=0, fd;
 
   char buff[11];
   char *pl_buff;
   struct message_s REQUEST;
   fd= *(int*)sd;
   //printf("in option\n");
   //printf("fd = %d\n",*fd);

	SSL *ssl;	
	ssl = SSL_new(sslctx);
	if (ssl == NULL) {
		fprintf(stderr, "ERR: unable to create the ssl structure\n");
		exit(-1);
	}

	/* Assign the socket into the SSL structure (SSL and socket without BIO) */
	SSL_set_fd(ssl, fd);

	/* Perform SSL Handshake on the SSL server */
	if( SSL_accept(ssl) == -1) {
		ERR_print_errors_fp(stderr); 
		exit(1);
	}
	printf("SSL connection using %s\n", SSL_get_cipher (ssl));

   if((len=recvn(ssl,buff, 10))<0){
   		printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
   }

   memcpy(&REQUEST, buff, 10);
   REQUEST.length = ntohl(REQUEST.length);
   // printf("REQUEST recved: %d\n",REQUEST.length);
   // printf("REQUEST.protocol:%s %d\n",REQUEST.protocol,memcmp(&REQUEST.protocol,"myftp",5));
   // printf("REQUEST.type: %x\n",REQUEST.type);
   // printf("REQUEST.length:%d %d\n",REQUEST.length,len);
   if(REQUEST.length > 10) {
   		pl_buff = malloc(sizeof(char) * (REQUEST.length-10));
		if((len=recvn(ssl, pl_buff, REQUEST.length-10))<0){
			printf("receive error: %s (Errno:%d)\n", strerror(errno),errno); exit(0);
   		}
   }
   //printf("outside\n");
   //printf("\nbuff: %s\n\n",buff);

   if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xA1){ //list
   		list(ssl);
   }
 
   else if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xB1){//get
   		get(ssl, pl_buff);
   }
   else if(memcmp(&REQUEST.protocol,"myftp",5)==0 && REQUEST.type == 0xC1){//put
	if(REQUEST.length != 0)
   		put(ssl, pl_buff);
   }
   else{
   	perror("server request failure\n");
   	exit(1);
   }

	if (SSL_shutdown(ssl) == -1) {
		ERR_print_errors_fp(stderr); 
		exit(1);	
	}
	if (close(fd) == -1) {
		perror("close");
		exit(-1);
	}
	SSL_free(ssl);
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

   if((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){ // Create a TCP Socket
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
        if(pthread_create(&thread[i], NULL, (void *)option, &accept_fd[i])!=0){
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
 
   if(argc != 2){
   	fprintf(stderr, "Usage: %s [port]\n", argv[0]);
   	exit(1);
   }
 
   port = atoi(argv[1]);

 	sslctx = sslctx_server();

   main_loop(port);

	SSL_CTX_free(sslctx);
   return 0;
}
 
 
