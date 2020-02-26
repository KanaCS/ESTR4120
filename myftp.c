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
	int i;
	char *buf_ = (char *)buf;
	for(i = 0; i < buf_len; i++) {
		buf_[i] = htonl(buf_[i]);
	}
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
	int i;
	char *buf_ = (char *)buf;
	for(i = 0; i < buf_len; i++) {
		buf_[i] = ntohl(buf_[i]);
	}
	return buf_len;
}

void showLoaderBytes(char *pre, char *str, unsigned long long b) {
	double t = (double)b /1024,  tprev = (double)b;
	int i = 0;
	while(tprev >= 1024) {
		tprev = t;
		t = t / 1024;
		i++;
	}
	// printf("\n%d\n", strlen(pre));
	memcpy(str, pre, strlen(pre));
	if(i == 1) {
		sprintf(&str[strlen(pre)], "%.2lf KB\0", tprev);
		return;
	}
	if(i == 2) {
		sprintf(&str[strlen(pre)], "%.2lf MB\0", tprev);
		return;
	}
	if(i == 3) {
		sprintf(&str[strlen(pre)], "%.2lf GB\0", tprev);
		return;
	}
	if(i == 4) {
		sprintf(&str[strlen(pre)], "%.2lf TB\0", tprev);
		return;
	}
	sprintf(&str[strlen(pre)], "%llu Bytes\0", b);
	return;
}
