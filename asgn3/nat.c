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
#include <pthread.h>
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
NAT_TB* nat_table = NULL;

pthread_mutex_t bucket_mutex;
TokenBucket bucket;

pthread_mutex_t packet_buffer_mutex;
unsigned int packets_num_in_buffer = 0;

pthread_t translation_thread;

int add_token(unsigned int num) {
  pthread_mutex_lock(&bucket_mutex);
  if(bucket.tokens < bucket.size) {
    bucket.tokens += num;
  }
  pthread_mutex_unlock(&bucket_mutex);
  return 1;
}

int get_token() { // return when get 1 token successfully
  struct timespec tim1, tim2;
  double fill_time_second = 1.0 / bucket.rate;
  fill_time_second = fill_time_second/2;
  tim1.tv_sec = (int)fill_time_second;
  fill_time_second -= (int)fill_time_second;
  tim1.tv_nsec = (long)(fill_time_second * 1E9);

  int result = 0;
  while(result == 0) {
    pthread_mutex_lock(&bucket_mutex);
    if(bucket.tokens > 0) {
      bucket.tokens -= 1;
      result = 1;
    }
    pthread_mutex_unlock(&bucket_mutex);
    if(result == 0) {
      if(nanosleep(&tim1, &tim2) < 0) {
        printf("ERROR: nanosleep() system call failed in get_token()!\n");
        exit(1);
      }
    }
  }
  return result;
}

void *token_bucket_thread_run(void *arg) {
  struct timespec tim1, tim2;
  double fill_time_second = 1.0 / bucket.rate;
  tim1.tv_sec = (int)fill_time_second;
  fill_time_second -= (int)fill_time_second;
  tim1.tv_nsec = (long)(fill_time_second * 1E9);
  // printf("nanotime = %ld\n", tim1.tv_nsec);
  while(add_token(1)) {
    if(nanosleep(&tim1, &tim2) < 0) {
      printf("ERROR: nanosleep() system call failed in token_bucket_thread_run()!\n");
      exit(1);
    }
  }
}

int search_dst_port(int dst_port, struct in_addr* targetip, int* targetport ){
  while(nat_table!=NULL){
    if(nat_table->trans_port == dst_port) {
      *targetip = nat_table->itn_ip;
      *targetport = nat_table->itn_port;
      return 1;
    }
    nat_table = nat_table->next;
  }
  return -1;
}

typedef struct trans_arg{
  unsigned int id;
  struct nfq_q_handle *myQueue;
  nfq_data* pkt;
  nfqnl_msg_packet_hdr *header;
} TransArg;

void *translation_thread_run(void *arg) {
  TransArg *param = (TransArg *)arg;
  unsigned int id = param->id;
  struct nfq_q_handle *myQueue = param->myQueue;
  nfq_data* pkt = param->pkt;
  nfqnl_msg_packet_hdr *header = param->header;

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
    int dst_port = udph->uh_dport;
    struct in_addr targetip;
    int targetport;
    if(search_dst_port(dst_port, &targetip, &targetport)==-1){
      printf("dst_port not in table, drop the pkt\n");
      return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
    }
    memcpy(&(iph->daddr),&targetip,sizeof(targetip));
    udph->uh_dport = htons(targetport);

    iph->check = ip_checksum(pktData);
    udph->check = udp_checksum(pktData);

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

  // check whether packet buffer have space left (buffer max = 10, to limit number of threads)
  int success = 0;
  pthread_mutex_lock(&packet_buffer_mutex);
  if(packets_num_in_buffer < 10) {
    packets_num_in_buffer += 1;
    success = 1;
  }
  else {
    printf("packet dropped due to limited buffer\n");
  }
  pthread_mutex_unlock(&packet_buffer_mutex);
  if(success == 0) {
    // drop this packet at router
    return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
  }

  // packet will goto new thread and get translated then set_verdict accordingly
  TransArg *arg = (TransArg *)malloc(sizeof(TransArg));
  arg->header = header;
  arg->id = id;
  arg->myQueue = myQueue;
  arg->pkt = pkt;
  pthread_create(&translation_thread, NULL, translation_thread_run, arg);
  return 0; // or required certain return?
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

  // setup token bucket
  bucket.size = bsize;
  bucket.rate = rate;
  bucket.tokens = bsize;

  // setup bucket mutex
  if (pthread_mutex_init(&bucket_mutex, NULL) < 0) { 
    printf("bucket_mutex init failed\n"); 
    exit(-1);
  } 

  if (pthread_mutex_init(&packet_buffer_mutex, NULL) < 0) { 
    printf("packet_buffer_mutex init failed\n"); 
    exit(-1);
  } 
  // start filling bucket thread
  pthread_t bucket_thread;
  pthread_create(&bucket_thread, NULL, token_bucket_thread_run, NULL);
  
  while ((res = recv(fd, buf, sizeof(buf), 0)) && res >= 0) {
    nfq_handle_packet(nfqHandle, buf, res);
  }

  nfq_destroy_queue(nfQueue);
  nfq_close(nfqHandle);
  pthread_mutex_destroy(&bucket_mutex); 
}
