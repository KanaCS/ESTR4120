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
	unsigned int itn_port;
	struct in_addr trans_ip;
	unsigned int trans_port;
	struct timeval accesstv;
	struct nat_tb* next;
} typedef NAT_TB;

typedef struct tokenbucket {
  unsigned int size;
  int tokens;
  unsigned int rate;
  struct timeval fill_base_tv;
} TokenBucket;
