#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <sys/time.h> // required by gettimeofday()
#include <time.h> // required by nanosleep()
#include <pthread.h>

// #define IPADDR "127.0.0.1"
// #define PORT 12345
#define PKT_MAX 1000

typedef struct sender_thread_args{
  int sd;
  struct sockaddr_in server_addr;
  struct timespec tim1;
  struct timeval *tvs;
} SendThdArgs;

void *sender_thread_run(void *args) {
  SendThdArgs *params = (SendThdArgs *)args;
  int sd = params->sd;
  struct sockaddr_in server_addr = params->server_addr;
  struct timespec tim1 = params->tim1;
  struct timeval *tvs = params->tvs;
  unsigned int counter;
  char buff[12];
  int len;

  socklen_t addrLen = sizeof(server_addr);
  struct timespec tim2;
  for (counter = 0; counter < PKT_MAX; counter++) {
    sprintf(buff, "%u", counter);
    if ((len = sendto(sd, buff, strlen(buff), 0,
                      (struct sockaddr *)&server_addr, addrLen)) <= 0) {
      printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }
    gettimeofday(&tvs[counter], NULL);
    printf("sent pkt%s\n", buff);
    if(nanosleep(&tim1, &tim2) < 0) {
      printf("ERROR: nanosleep() system call failed!\n");
      exit(1);
    }
  }
}

typedef struct recver_thread_args{
  int sd;
  struct sockaddr_in server_addr;
  struct timeval *tvs;
} RecvThdArgs;

void *recver_thread_run(void *args) {
  RecvThdArgs *params = (RecvThdArgs *)args;
  int sd = params->sd;
  struct timeval *tvs = params->tvs;
  struct sockaddr_in server_addr = params->server_addr;
  char recvBuff[100];
  int len;
  struct sockaddr_in client_addr;
  unsigned int counter = 0;
  int pkt_num;
  struct timeval curr_tv;
  char ip_str[INET_ADDRSTRLEN];
  socklen_t addrLen = sizeof(server_addr);
  while(1) {
    if ((len = recvfrom(sd, recvBuff, sizeof(recvBuff), 0, (struct sockaddr *)&client_addr, &addrLen)) <= 0) { 
      printf("recv Error: %s (Errno:%d)\n", strerror(errno), errno);
    }
    else {
      recvBuff[len] = '\0';
      pkt_num = atoi(recvBuff + 4);
      gettimeofday(&curr_tv, NULL);
      inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
      printf("recved response from server (%s, %d): %s, rtt: %.6lf s\n", ip_str, ntohs(client_addr.sin_port), recvBuff, (curr_tv.tv_sec - tvs[pkt_num].tv_sec) + (curr_tv.tv_usec - tvs[pkt_num].tv_usec)/ 1.0E6 );
    }
  }
}

int main(int argc, char **argv) {
  if(argc != 4){
    fprintf(stderr, "Usage: ./client <IP> <PORT> <SEND_RATE>\n");
    exit(1);
  }
  char *ip_addr = argv[1];
  char *port = argv[2];

  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip_addr);
  server_addr.sin_port = htons(atoi(port));
  socklen_t addrLen = sizeof(server_addr);

  unsigned int counter = 0;
  struct timeval tvs[PKT_MAX];

  struct timespec tim1;
  double rate = atof(argv[3]);
  double send_time = 1/rate;
  tim1.tv_sec = (int)(send_time);
  send_time -= (int)(send_time);
  tim1.tv_nsec = (long)(send_time * 1E9);

  SendThdArgs s_arg;
  s_arg.sd = sd;
  s_arg.server_addr = server_addr;
  s_arg.tim1 = tim1;
  s_arg.tvs = tvs;
  pthread_t sender_thread;
  pthread_create(&sender_thread, NULL, sender_thread_run, (void *)&s_arg);

  RecvThdArgs r_arg;
  r_arg.sd = sd;
  r_arg.server_addr = server_addr;
  r_arg.tvs = tvs;
  pthread_t recver_thread;
  pthread_create(&recver_thread, NULL, recver_thread_run, (void *)&r_arg);
  pthread_join(recver_thread, NULL);
  close(sd);
  return 0;
}
