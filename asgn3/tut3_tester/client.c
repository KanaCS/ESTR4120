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

// #define IPADDR "127.0.0.1"
// #define PORT 12345

int main(int argc, char **argv) {
  if(argc != 4){
    fprintf(stderr, "Usage: ./client <IP> <PORT> <SEND_RATE>\n");
    exit(1);
  }
  char *ip_addr = argv[1];
  char *port = argv[2];
  /* if(connect(sd,(struct sockaddr *)&server_addr,sizeof(server_addr))<0){ */
  /*     printf("connection error: %s (Errno:%d)\n",strerror(errno),errno); */
  /*     exit(0); */
  /* } */
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip_addr);
  server_addr.sin_port = htons(atoi(port));
  socklen_t addrLen = sizeof(server_addr);

  char recvBuff[100];
  char buff[12];
  struct sockaddr_in client_addr;
  unsigned int counter = 0;
  struct timeval *tv = malloc(sizeof(struct timeval));
  struct timeval *prev_tv = malloc(sizeof(struct timeval));

  struct timespec tim1, tim2;
  double rate = atof(argv[3]);
  double send_time = 1/rate;
  tim1.tv_sec = (int)(send_time);
  send_time -= (int)(send_time);
  tim1.tv_nsec = (long)(send_time * 1E9);

  for (counter = 0; counter < UINT_MAX; counter++) {
    sprintf(buff, "%u", counter);
    int len;
    if ((len = sendto(sd, buff, strlen(buff), 0,
                      (struct sockaddr *)&server_addr, addrLen)) <= 0) {
      printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
      exit(0);
    }
    /** Even directly use the following "if" is okay. Knowing IP addr just
     * decides who sends data first*/
    //    if ((len = recvfrom(sd, recvBuff, sizeof(recvBuff), 0, NULL, NULL)) <=
    //    0) {
    if ((len = recvfrom(sd, recvBuff, sizeof(recvBuff), 0,
                        (struct sockaddr *)&client_addr, &addrLen)) <= 0) {
      printf("recv Error: %s (Errno:%d)\n", strerror(errno), errno);
    } 
    else {
      recvBuff[len] = '\0';
      if(counter > 1) {
        prev_tv->tv_sec = tv->tv_sec;
        prev_tv->tv_usec = tv->tv_usec;
        gettimeofday(tv, NULL);
        printf("recved response from server: %s, diff time: %.6lf s\n", recvBuff, (tv->tv_sec - prev_tv->tv_sec) + (tv->tv_usec - prev_tv->tv_usec)/ 1.0E6 );
      }
      else {
        if(counter == 0) {
          gettimeofday(tv, NULL);
        }
        else {
          prev_tv->tv_sec = tv->tv_sec;
          prev_tv->tv_usec = tv->tv_usec;
          gettimeofday(tv, NULL);
        }
        printf("recved response from server: %s\n", recvBuff);
      }
    }
    if(nanosleep(&tim1, &tim2) < 0) {
      printf("ERROR: nanosleep() system call failed!\n");
      exit(1);
    }
  }
  free(tv); free(prev_tv);
  close(sd);
  return 0;
}
