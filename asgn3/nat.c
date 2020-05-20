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
#include "nat.h"
extern "C" {
#include <linux/netfilter.h> // required by NF_ACCEPT, NF_DROP, etc...
#include <libnetfilter_queue/libnetfilter_queue.h>
}

#include "checksum.h"

#define BUF_SIZE 1500

struct in_addr ip;
struct in_addr lan;
int mask=0;

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg,
                nfq_data* pkt, void *cbData) {
  unsigned int id = 0;
  nfqnl_msg_packet_hdr *header;

  printf("pkt recvd: ");
  if ((header = nfq_get_msg_packet_hdr(pkt))) {
          id = ntohl(header->packet_id);
          printf("  id: %u\n", id);
          printf("  hw_protocol: %u\n", ntohs(header->hw_protocol));       
          printf("  hook: %u\n", header->hook);
  }

  unsigned char *pktData;
  int len = nfq_get_payload(pkt, (unsigned char**)&pktData);
  struct iphdr *iph = (struct iphdr *)pktData;
  struct udphdr *udph =(struct udphdr *) (((char*)iph) + iph->ihl*4);
  unsigned char* appData = pktData + iph->ihl * 4 + 8;

  if (iph->protocol != IPPROTO_UDP) {
      printf("drop pkt other than udp\n");
      return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
  }
  unsigned int local_mask = 0xffffffff << (32 - mask);
  unsigned int lan_int = ntohl(lan.s_addr);
  unsigned int local_network = local_mask & lan_int;  
printf("%d \t %d\n",local_network,ntohl(iph->saddr));

  if ((ntohl(iph->saddr) & local_mask) == local_network) {
	printf("outbound\n");  
	// outbound traffic
	//modify source IP to the public IP of gateway
	//allocate a port
  } else {
	printf("inbound\n");
	// inbound traffic
	//convert the (DestIP, port) to the original (IP, port)
  }
/*
  // print the timestamp (PC: seems the timestamp is not always set)
  struct timeval tv;
  if (!nfq_get_timestamp(pkt, &tv)) {
          printf("  timestamp: %lu.%lu\n", tv.tv_sec, tv.tv_usec);
  } else {
          printf("  timestamp: nil\n");
  }
*/
  // Print the payload; in copy meta mode, only headers will be
  // included; in copy packet mode, whole packet will be returned.
  printf(" payload: ");
  if (len > 0) {
          for (int i=0; i<len; ++i) {
                  printf("%02x ", pktData[i]);
          }
  }
  printf("\n");

  return nfq_set_verdict(myQueue, id, NF_ACCEPT, 0, NULL);
}


int main(int argc, char** argv) {

  if(argc != 6){
    fprintf(stderr, "Usage: sudo ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>\n");
    exit(1);
  }
  char ip_s[30],lan_s[30];
  strcpy(ip_s,argv[1]);
  strcpy(lan_s,argv[2]);
  inet_aton(ip_s, &ip);
  inet_aton(lan_s, &lan);
  mask = atoi(argv[3]);
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
  
  // Bind this handler to process IP packets...
  if (nfq_bind_pf(nfqHandle, AF_INET) < 0) {
    fprintf(stderr, "Error in nfq_bind_pf()\n");
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
