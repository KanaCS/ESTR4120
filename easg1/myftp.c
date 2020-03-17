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
	// bytes reordering part
	char *buf_ = (char *)buf;
	uint16_t *temp = malloc(sizeof(uint16_t));
	*temp = 123;
//	if (*temp != htons(*temp)) { // need to flip
//		if (buf_len % 2 == 0) {
//			int step = sizeof(uint16_t);
//			for(i = 0; i < buf_len; i+=step) {
//				memcpy(temp, &buf_[i], step);
//				*temp = htons(*temp);
//				memcpy(&buf_[i], temp, step);
//			}
//		}
//		else {
//			int step = sizeof(uint16_t);
//			for(i = 0; i < buf_len-1; i+=step) {
//				memcpy(temp, &buf_[i], step);
//				*temp = htons(*temp);
//				memcpy(&buf_[i], temp, step);
//			}
//			memcpy(temp, &buf_[buf_len-step], step);
//			*temp = htons(*temp);
//			memcpy(&buf_[buf_len-step/2], temp, step/2);
//		}
//	}
	// 12345678
	// 87654321
	// bytes reordering part end
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
	free(temp);
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
	// bytes reordering part
	char *buf_ = (char *)buf;
	uint16_t *temp = malloc(sizeof(uint16_t));
//	*temp = 123;
//	if (*temp != ntohs(*temp)) { // need to flip
//		if (buf_len % 2 == 0) {
//			int step = sizeof(uint16_t);
//			for(i = 0; i < buf_len; i+=step) {
//				memcpy(temp, &buf_[i], step);
//				*temp = ntohs(*temp);
//				memcpy(&buf_[i], temp, step);
//			}
//		}
//		else {
//			int step = sizeof(uint16_t);
//			for(i = 0; i < buf_len-1; i+=step) {
//				memcpy(temp, &buf_[i], step);
//				*temp = ntohs(*temp);
//				memcpy(&buf_[i], temp, step);
//			}
//			memcpy(temp, &buf_[buf_len-step], step);
//			*temp = ntohs(*temp);
//			memcpy(&buf_[buf_len-step/2], temp, step/2);
//		}
	//}
	// bytes reordering part end

	free(temp);
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
