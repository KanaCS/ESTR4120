#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> // required by "netfilter.h"
#include <arpa/inet.h> // required by ntoh[s|l]()
#include <signal.h> // required by SIGINT
#include <string.h> // required by strerror()
#include <sys/time.h> // required by gettimeofday()
#include <time.h> // required by nanosleep()
#include <errno.h> // required by errno
# include <pthread.h>
#include <netinet/ip.h>        // required by "struct iph"
#include <netinet/tcp.h>    // required by "struct tcph"
#include <netinet/udp.h>    // required by "struct udph"
#include <netinet/ip_icmp.h>    // required by "struct icmphdr"

struct nat_tb{
	struct in_addr itn_ip;
	int itn_port;
	struct in_addr trans_ip;
	int trans_port;
	struct timeval accesstv;
	struct nat_tb* next;
} typedef NAT_TB;

typedef struct tokenbucket {
  unsigned int size;
  unsigned int tokens;
  unsigned int rate;
} TokenBucket;

void print_nat_tb(NAT_TB *tb) {
	print("\n");
	printf("|\t%-15s\t|\t%-12s\t|\t%-15s\t|\t%-16s\t|\t%-13s\t|\n", "Source IP", "Source Port", "Translated IP", "Translated Port", "Time To Live")
	while(tb != NULL) {
		print_nat_tb_i(tb);
		tb = tb->next;
	}
}
void print_nat_tb_i(NAT_TB *tb) {
	char itn_ip_str[INET_ADDRSTRLEN];
	char trans_ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(tb->itn_ip.s_addr), itn_ip_str, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &(tb->trans_ip.s_addr), trans_ip_str, INET_ADDRSTRLEN);
	printf("|\t%-15s\t|\t%-12d\t|\t%-15s\t|\t%-16d\t|\t%-13.3lf\t|\n", itn_ip_str, tb->itn_port, trans_ip_str, tb->trans_ip, (double)(tb->accesstv.tv_sec) + (double)(tb->accesstv.tv_usec) / 10E6);
}