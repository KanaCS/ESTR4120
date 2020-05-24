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
#define MAX_PKT_IN_BUF 10

struct in_addr natip;
struct in_addr natlan;
int mask=0;
NAT_TB* nat_table = NULL;
char porttb[2000];

pthread_mutex_t bucket_mutex;
TokenBucket bucket;

pthread_mutex_t buf_mutex;
int pkt_in_buf = 0;
pthread_t recv_thread;


void remove_expiry(){
  NAT_TB* table = nat_table;
  NAT_TB* prev = NULL;
  struct timeval  tv;
  gettimeofday(&tv, NULL);
  unsigned long now = (tv.tv_sec) * 1000 ;//+ (tv.tv_usec) / 1000 ;
  while(table!=NULL){
    unsigned long pkttime = (table->accesstv.tv_sec) * 1000 ;//+ (table->accesstv.tv_usec) / 1000 ;
    if (now - pkttime >= 10000){
      pthread_mutex_lock(&buf_mutex);
      printf("drop a expiry pkt\n");
      printf("true? %d\n",table->next==NULL);
      NAT_TB* befree = NULL;
      if(prev == NULL){
      	nat_table = table->next;
      	befree = table;
      	table = nat_table;
      }else{
        prev->next = table->next;
      	befree = table;
      	table = prev->next;
      }
      if(befree!=NULL){
	      porttb[befree->trans_port-10000] = 'n';
        free(befree);
      }
      pthread_mutex_unlock(&buf_mutex);
    }
    else{
    	prev = table;
	    table = table->next;
    }
  }
// printf("finish remove\n");
}

void printNAT(){
  NAT_TB* table = nat_table;
  printf("------------------------------------------NAT TABLE----------------------------------------------\n");
  while(table!=NULL){
    struct in_addr tmp = table->itn_ip;
    tmp.s_addr = htonl(tmp.s_addr);
    printf("src_ip:%s\t",inet_ntoa(tmp)); 
    printf("src_p:%u\t",table->itn_port);
    struct in_addr tmpt = table->trans_ip;
    tmpt.s_addr = htonl(tmpt.s_addr);
    printf("trans_ip:%s\t", inet_ntoa(tmpt));
    printf("trans_p:%u\t", table->trans_port);
    printf("timestamp: %lu.%lu\n", table->accesstv.tv_sec, table->accesstv.tv_usec);
    table = table->next;
  }
  printf("-------------------------------------------------------------------------------------------------\n");
}


// int add_token(unsigned int num) {
//   pthread_mutex_lock(&bucket_mutex);
//   if(bucket.tokens < bucket.size) {
//     bucket.tokens += num;
//   }
//   pthread_mutex_unlock(&bucket_mutex);
//   return 1;
// }

double time_diff(struct timeval tv1, struct timeval tv2) {
  double t1 = tv1.tv_sec + tv1.tv_usec / 1.0E6;
  double t2 = tv2.tv_sec + tv2.tv_usec / 1.0E6;
  return t1-t2;
}

void forward_seconds(struct timeval *tv, double s) {
  int sec = (int)s;
  unsigned int usec = (s-sec) * 1E6;
  tv->tv_sec += sec;
  if(tv->tv_usec + usec > 1E6) {
    tv->tv_sec += 1;
    tv->tv_usec = tv->tv_usec + usec - 1E6;
  }
  else {
    tv->tv_usec += usec;
  }
}

void lazy_refill(struct timeval curr_tv, double fill_time) {
  double time_from_base = time_diff(curr_tv, bucket.fill_base_tv);
  int fill_tokens = time_from_base / fill_time;
  if(bucket.tokens + fill_tokens >= bucket.size) {
    // bucket already full, taking one away and set start filling
    bucket.tokens = bucket.size;
    gettimeofday(&bucket.fill_base_tv, NULL);
  }
  else {
    // bucket not yet full, update fill base
    bucket.tokens += fill_tokens;
    forward_seconds(&bucket.fill_base_tv, fill_time * fill_tokens);
  }
}

void get_token() { // return when get 1 token successfully
  struct timespec tim1, tim2;
  double fill_time = 1.0 / bucket.rate;
  int fill_time_sec = (int)fill_time;
  unsigned int fill_time_nsec = (fill_time - fill_time_sec) * 1E9;

  int sleepy = 0;
  struct timeval curr_tv;
  pthread_mutex_lock(&bucket_mutex);
  printf("%d tokens in bucket\n", bucket.tokens);
  if(bucket.tokens > 0) {
    // have token
    if(bucket.tokens < bucket.size) {
      // lazy refill here
      gettimeofday(&curr_tv, NULL);
      lazy_refill(curr_tv, fill_time);
    }
    else {
      // bucket start filling, only reached by the 1st pkt
      printf("first pkt setup fill base\n");
      gettimeofday(&bucket.fill_base_tv, NULL);
    }
    bucket.tokens -= 1;
  }
  else {
    // no tokens
    struct timeval next_token_tv;
    next_token_tv = bucket.fill_base_tv;
    forward_seconds(&next_token_tv, fill_time);
    // printf("%ld.%ld -> %ld.%ld\n", bucket.fill_base_tv.tv_sec, bucket.fill_base_tv.tv_usec, next_token_tv.tv_sec, next_token_tv.tv_usec);
    double td;
    gettimeofday(&curr_tv, NULL);
    if((td=time_diff(next_token_tv, curr_tv)) > 0.0) {
      // need to wait for the next token
      tim1.tv_sec = (int)td;
      tim1.tv_nsec = (td-tim1.tv_sec) * 1E9;
      sleepy = 1;
      forward_seconds(&bucket.fill_base_tv, fill_time);
      // printf("curr: %ld.%ld, next token avaliable at %ld.%ld\n", curr_tv.tv_sec, curr_tv.tv_usec, next_token_tv.tv_sec, next_token_tv.tv_usec);
      printf("this pkt will wait %.6lf s for a token\n", td);
    }
    else {
      // already have token in bucket, lazy refill here
      lazy_refill(curr_tv, fill_time);
      bucket.tokens -= 1;
    }
  }
  pthread_mutex_unlock(&bucket_mutex);
  if(sleepy == 1) {
    if(nanosleep(&tim1, &tim2) < 0) {
      printf("ERROR: nanosleep() system call failed in get_token()!\n");
      exit(1);
    }
  }
}

// void *token_bucket_thread_run(void *arg) {
//   struct timespec tim1, tim2;
//   double fill_time_second = 1.0 / bucket.rate;
//   tim1.tv_sec = (int)fill_time_second;
//   fill_time_second -= (int)fill_time_second;
//   tim1.tv_nsec = (long)(fill_time_second * 1E9);
//   // printf("nanotime = %ld\n", tim1.tv_nsec);
//   while(add_token(1)) {
//     if(nanosleep(&tim1, &tim2) < 0) {
//       printf("ERROR: nanosleep() system call failed in token_bucket_thread_run()!\n");
//       exit(1);
//     }
//   }
// }

u_short search_dst_port(u_short dst_port, struct in_addr* targetip, u_short* targetport, struct timeval tv ){
  NAT_TB* table = nat_table;
  while(table!=NULL){
    if(table->trans_port == dst_port) {
      table->accesstv = tv;
      *targetip = table->itn_ip;
      *targetport = table->itn_port;
      return 1;
    }
    table = table->next;
  }
  return -1;
}

unsigned int search_entry(struct in_addr ip, u_short port, struct timeval tv){ 
  // printf("in search_entry arrrr\n");
  NAT_TB* table = nat_table;

  //initial null case
  if(nat_table == NULL){
      // printf("initial the table arrrrr\n");
      nat_table = (NAT_TB*)malloc(sizeof(NAT_TB));
      nat_table->itn_ip = ip;
      nat_table->itn_port = port;
      nat_table->trans_ip = natip;
      nat_table->next = NULL;
      int i = 0;
      for(i=0;i<2000;i++){
	if(porttb[i]=='n'){
	  porttb[i]='y';
	  break;
	}
      }
      if (i>=2000){
	printf("no port available\n"); exit(-1);
      }
      nat_table->trans_port = i+10000;
      nat_table->accesstv = tv;
      return nat_table->trans_port;
  }
  //traverse tb
  while(table!=NULL){
    //found entry
    if(table->itn_port == port && table->itn_ip.s_addr == ip.s_addr) {
      table->accesstv = tv;
      return table->trans_port;
    }//not found, new a entry
    else if(table->next==NULL){
      table->next = (NAT_TB*)malloc(sizeof(NAT_TB));
      table = table->next;
      table->next = NULL;
      table->itn_ip = ip;
      table->itn_port = port;
      table->trans_ip = natip;
      int i = 0;
      for(i=0;i<2000;i++){
        if(porttb[i]=='n'){
          porttb[i]='y';
          break;
        }
      }
      if (i>=2000){
        printf("no port available\n"); exit(-1);
      }
      table->trans_port = i+10000;
      table->accesstv = tv;
      return table->trans_port;
    }
    table = table->next;
  }
  return 0;
}

typedef struct recv_thread_arg{
  struct nfq_handle *nfqHandle;
  char buf[BUF_SIZE];
  int res;
} RecvThreadArg;

void *recv_thread_run(void *arg) {
  RecvThreadArg *params = (RecvThreadArg *)arg;
  nfq_handle_packet(params->nfqHandle, params->buf, params->res);
  free(params);
}

int add_pkt_buf() {
  int res = 0;
  pthread_mutex_lock(&buf_mutex);
  if(pkt_in_buf < MAX_PKT_IN_BUF) {
    pkt_in_buf += 1;
    res = pkt_in_buf;
  }
  pthread_mutex_unlock(&buf_mutex);
  return res;
}

int rm_pkt_buf() {
  pthread_mutex_lock(&buf_mutex);
  pkt_in_buf -= 1;
  pthread_mutex_unlock(&buf_mutex);
  return 1;
}

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg,
                nfq_data* pkt, void *cbData) {
  unsigned int id = 0;
  nfqnl_msg_packet_hdr *header;
  printf("\npkt recvd: ");
  if ((header = nfq_get_msg_packet_hdr(pkt))) {
          id = ntohl(header->packet_id);
          printf("  id: %u\n", id);
          // printf("  hw_protocol: %u\n", ntohs(header->hw_protocol));       
          // printf("  hook: %u\n", header->hook);
  }

  unsigned char *pktData;
  int len = nfq_get_payload(pkt, (unsigned char**)&pktData);
  struct iphdr *iph = (struct iphdr *)pktData;
  struct udphdr *udph =(struct udphdr *) (((char*)iph) + iph->ihl*4);
  //unsigned char* appData = pktData + iph->ihl * 4 + 8;

  if (iph->protocol != IPPROTO_UDP) {
      printf("drop pkt other than udp\n");
      return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
  }
  int pkt_buf_cnt;
  if ((pkt_buf_cnt = add_pkt_buf()) == 0) {
    printf("!!!! drop pkt because pkt_in_buf > 10 !!!!\n");
    return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
  }
  else {
    printf("pkt_buf_cnt: %d\n", pkt_buf_cnt);
  }
  unsigned int local_mask = 0xffffffff << (32 - mask);
  unsigned int lan_int = natlan.s_addr;
  unsigned int local_network = local_mask & lan_int;  
  struct timeval tv;
  if (!nfq_get_timestamp(pkt, &tv)) {
    // printf("  timestamp: %lu.%lu\n", tv.tv_sec, tv.tv_usec);   
  } else {
    gettimeofday(&tv, NULL);
    // printf("  nil timestamp: %lu.%lu\n", tv.tv_sec, tv.tv_usec);
  }

  //lazy approach to remove expired entries
  remove_expiry();

  if ((ntohl(iph->saddr) & local_mask) == local_network){
    printf("outbound\n");  
    struct in_addr ipadr;
    ipadr.s_addr = ntohl(iph->saddr);
    u_short targetport = search_entry(ipadr, ntohs(udph->source), tv);
    udph->source = htons(targetport);
    iph->saddr = htonl(natip.s_addr);
    udph->check = udp_checksum(pktData);
    iph->check = ip_checksum(pktData);
    printNAT();
   } 
  else {
    printf("inbound\n");
    u_short dst_port = ntohs(udph->dest);
    struct in_addr targetip;
    u_short targetport;
    if(search_dst_port(dst_port, &targetip, &targetport, tv)!=1){
      printf("dst_port not in table, drop the pkt\n");
      rm_pkt_buf();
      return nfq_set_verdict(myQueue, id, NF_DROP, 0, NULL);
    }
    iph->daddr = htonl(targetip.s_addr);
    udph->dest = htons(targetport);
    udph->check = udp_checksum(pktData);
    iph->check = ip_checksum(pktData);
    printNAT();
  }

  // printf("\n");
  get_token(); // wait for token
  rm_pkt_buf(); // reduce pkt_in_buf by 1
  return nfq_set_verdict(myQueue, id, NF_ACCEPT, len, pktData);
}

int main(int argc, char** argv) {

  if(argc != 6){
    fprintf(stderr, "Usage: sudo ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>\n");
    exit(1);
  }
  char ip_s[30],lan_s[30];
  strcpy(ip_s,argv[1]);
  strcpy(lan_s,argv[2]);
  inet_aton(ip_s, &natip);
  natip.s_addr = ntohl(natip.s_addr);
  inet_aton(lan_s, &natlan);
  natlan.s_addr = ntohl(natlan.s_addr);

  mask = atoi(argv[3]);
  int bsize = atoi(argv[4]);
  int rate = atoi(argv[5]);
printf("############# initial : %lu\n",natip.s_addr); 
  //init porttb
  int i = 0;   
  for(i = 0; i<2000;i++){
    porttb[i]='n';
  }
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
  gettimeofday(&bucket.fill_base_tv, NULL);
  // setup bucket mutex
  if (pthread_mutex_init(&bucket_mutex, NULL) < 0) { printf("bucket_mutex init failed\n"); exit(-1);} 
  if (pthread_mutex_init(&buf_mutex, NULL) < 0) {  printf("buf_mutex init failed\n"); exit(-1);} 

  // start filling bucket thread
  // pthread_t bucket_thread;
  // pthread_create(&bucket_thread, NULL, token_bucket_thread_run, NULL);
  
  while ((res = recv(fd, buf, sizeof(buf), 0)) && res >= 0) {
    RecvThreadArg *args = (RecvThreadArg *)malloc(sizeof(RecvThreadArg));
    memcpy(args->buf, buf, res);
    args->res = res;
    args->nfqHandle = nfqHandle;
    pthread_create(&recv_thread, NULL, recv_thread_run, args); // pass the recv pkt to a new thread, allow main to continue recv next pkt.
  }

  nfq_destroy_queue(nfQueue);
  nfq_close(nfqHandle);
  pthread_mutex_destroy(&bucket_mutex); 
}
