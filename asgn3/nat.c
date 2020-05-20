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

extern "C" {
#include <linux/netfilter.h> // required by NF_ACCEPT, NF_DROP, etc...
#include <libnetfilter_queue/libnetfilter_queue.h>
}

#include "checksum.h"

#define BUF_SIZE 1500

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg,
    struct nfq_data *pkt, void *cbData) {
  // Get the id in the queue
  unsigned int id = 0;

  // Access IP Packet
  unsigned char *pktData;
  int ip_pkt_len = nfq_get_payload(pkt, &pktData);;
  struct iphdr *ipHeader = (struct iphdr *)pktData;;
  
  //check protocol
  if (iph->protocol == IPPROTO_UDP) {
  // UDP packets
    struct udphdr *udph = ;
  } else {
  // Others, can be ignored
    return ;
  }

  return nfq_set_verdict(myQueue, id, NF_ACCEPT, ip_pkt_len, pktData);
}


int main(int argc, char** argv) {

  if(argc != 7){
    fprintf(stderr, "Usage: sudo ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>\n");
    exit(1);
  }
  char ip[30];
  strcpy(ip,argv[1]);
  char lan[30];
  strcpy(lan,argv[2]);
  int mask = atoi(argv[3]);
  int bsize = atoi(argv[4]);
  int rate = atoi(argv[5]);

  // Get a queue connection handle from the module
  struct nfq_handle *nfqHandle;
  if (!(nfqHandle = nfq_open())) {
    fprintf(stderr, "Error in nfq_open()\n");
    exit(-1);
  }

  // Unbind the handler from processing any IP packets
  if (nfq_unbind_pf(nfqHandle, AF_INET) < 0) {
    fprintf(stderr, "Error in nfq_unbind_pf()\n");
    exit(1);
  }

  // Install a callback on queue 0
  struct nfq_q_handle *nfQueue;
  if (!(nfQueue = nfq_create_queue(nfqHandle,  0, &Callback, NULL))) {
    fprintf(stderr, "Error in nfq_create_queue()\n");
    exit(1);
  }
  // nfq_set_mode: I want the entire packet 
  if(nfq_set_mode(nfQueue, NFQNL_COPY_PACKET, BUF_SIZE) < 0) {
    fprintf(stderr, "Error in nfq_set_mode()\n");
    exit(1);
  }

  struct nfnl_handle *netlinkHandle;
  netlinkHandle = nfq_nfnlh(nfqHandle);

  int fd;
  fd = nfnl_fd(netlinkHandle);

  int res;
  char buf[BUF_SIZE];

  while ((res = recv(fd, buf, sizeof(buf), 0)) && res >= 0) {
    nfq_handle_packet(nfqHandle, buf, res);
  }

  nfq_destroy_queue(nfQueue);
  nfq_close(nfqHandle);

}
