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
#include <sys/wait.h>

int sendn(int sd, void *buf, int buf_len){
	int n_left = buf_len;
	int n;
	while(n_left > 0){
		if((n = send(sd, buf + (buf_len - n_left), n_left, 0)) < 0){
			if(errno == EINTR) n = 0;
			else return -1;
		}
		else if (n == 0) return 0;
		n_left -= n;
	}
	return buf_len;
}

int recvn(int sd, void *buf, int buf_len){
	int n_left = buf_len;
	int n;
	while(n_left > 0){
		if((n = recv(sd, buf + (buf_len - n_left), n_left, 0)) < 0){
			if(errno == EINTR) n = 0;
			else return -1;
		}
		else if (n == 0) return 0;
		n_left -= n;
	}
	return buf_len;
}