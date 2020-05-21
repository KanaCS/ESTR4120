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

// #define PORT 12345

int main(int argc, char **argv) {
  if(argc != 2){
    fprintf(stderr, "Usage: ./server <PORT>\n");
    exit(1);
  }
  char *port = argv[1];
  int sd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(atoi(port));
  socklen_t addrLen = sizeof(server_addr);
  if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    printf("bind error: %s (Errno:%d)\n", strerror(errno), errno);
    exit(0);
  }
  struct sockaddr_in client_addr;
  unsigned int counter;
  char ackBuff[16];
  char ip_str[INET_ADDRSTRLEN];
  struct timeval *tv = malloc(sizeof(struct timeval));
  struct timeval *prev_tv = malloc(sizeof(struct timeval));
  for (counter = 0; counter < UINT_MAX; counter++) {
    char buff[100];
    int len;
    sprintf(ackBuff, "ack%d", counter);
    // printf("Waiting heartbeat\n");
    if ((len = recvfrom(sd, buff, sizeof(buff), 0,
                        (struct sockaddr *)&client_addr, &addrLen)) <= 0) {
      printf("receive error: %s (Errno:%d)\n", strerror(errno), errno);
    }

    buff[len] = '\0';
    // printf("Received heartbeat: ");
    if (strlen(buff) != 0) {
      if(counter > 1) {
        prev_tv->tv_sec = tv->tv_sec;
        prev_tv->tv_usec = tv->tv_usec;
        gettimeofday(tv, NULL);
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        printf("recv from (%s, %d): %s, diff time: %.6lf s\n", ip_str, ntohs(client_addr.sin_port), buff, (tv->tv_sec - prev_tv->tv_sec) + (tv->tv_usec - prev_tv->tv_usec)/ 1.0E6 );
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
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        printf("recv from (%s, %d): %s\n", ip_str, ntohs(client_addr.sin_port), buff);
      }
    }
    if ((len = sendto(sd, ackBuff, strlen(ackBuff), 0,
                      (struct sockaddr *)&client_addr, addrLen)) <= 0) {
      printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
      // exit(0);
    }
  }
  free(tv); free(prev_tv);
  close(sd);
  return 0;
}
