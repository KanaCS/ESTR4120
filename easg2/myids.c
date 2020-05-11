#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>                                             
#include <pcap.h>                                             
#include <unistd.h>                                           
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define ETH_HDR_LEN 14

unsigned short checksum(unsigned char *ip_hdr)
{   
    char buf[20];  
    struct iphdr *iph; 
    memcpy(buf, ip_hdr, sizeof(buf));
    iph = (struct iphdr *) buf;
    iph -> check = 0;
        
    unsigned long sum = 0;
    u_short *ip;
    ip = (u_short*)buf;
    u_short hdr_len = 0;
    while (hdr_len < 20)
    {    
         sum += *ip++;
         if (sum >> 16){ 
                 u_short carry = sum & 0xFFFF;
                 sum = (sum >> 16) + carry;
         }
         hdr_len += 2;
    }
    if (sum >> 16){
         u_short carry = sum & 0xFFFF;
         sum = (sum >> 16) + carry;
    }
    return(~sum);
}

typedef struct element {
	struct element* next;
	unsigned int ip_addr;
	double cumu_byte_count; // in units of 10^6 Bytes
} Element;

double update(Element** table, unsigned int ip_addr, unsigned int payload_size) { // return cumulative byte count
	unsigned int ind = ip_addr % 10000;
	if(table[ind] == NULL) {
		// add new entry
		table[ind] = (Element *) malloc(sizeof(Element));
		table[ind]->next = NULL;
		table[ind]->ip_addr = ip_addr;
		table[ind]->cumu_byte_count = payload_size/1.0E6;
		return table[ind]->cumu_byte_count;
	}
	else {
		Element* target;
		target = table[ind];
		while(target->ip_addr != ip_addr) {
			if(target->next != NULL) {
				target = target->next;
			}
			else {
				// add new entry
				target->next = (Element *) malloc(sizeof(Element));
				target->next->next = NULL;
				target->next->ip_addr = ip_addr;
				target->next->cumu_byte_count = payload_size/1.0E6;
				return target->next->cumu_byte_count;
			}
		}
		// entry found, update byte count
		target->cumu_byte_count += payload_size/1.0E6;
		return target->cumu_byte_count;
	}
}

double query(Element** table, unsigned int ip_addr) {
	unsigned int ind = ip_addr % 10000;
	if(table[ind] == NULL) {
		return 0.0;
	}
	else {
		Element* target;
		target = table[ind];
		while(target->ip_addr != ip_addr) {
			if(target->next != NULL) {
				target = target->next;
			} 
			else {
				return 0.0;
			}
		}
		return target->cumu_byte_count;
	}
}

void free_helper(Element* t) {
	unsigned int length = 1;
	Element* head = t;
	while(t->next != NULL) {
		length++;
	}
	Element** table = (Element** )malloc(sizeof(Element* ) * length);
	int i;
	t = head;
	for(i = length-1; i > 0; i--) {
		table[i] = t;
		t = t->next;
	}
	table[0] = t;
	for(i = 0; i <length; i++) {
		free(table[i]);
	}
	free(table);
	// if(t->next == NULL) {
	// 	free(t);
	// }
	// else {
	// 	free_helper(t->next);
	// 	free(t);
	// }
}

void clear_table(Element** table) {
	int i;
	for(i = 0; i < 10000; i++) {
		if(table[i] != NULL) {
			free_helper(table[i]);
		}
	}
}

void print_ip(unsigned int ip) {
	print("%d.%d.%d.%d", (ip>>(8*3)) & 0xFF, (ip>>(8*2)) & 0xFF, (ip>>(8*1)) & 0xFF, ip & 0xFF);
}

/***************************************************************************
 * Main program
 ***************************************************************************/
int main(int argc, char** argv) {
	if(argc != 5){
		fprintf(stderr, "Usage: ./myids <hh_thresh> <hc_thresh> <ss_thresh> <epoch>");
		exit(1);
	}
	double hh_thresh = atof(argv[1]);
	double hc_thresh = atof(argv[2]);
	double ss_thresh = atof(argv[3]);
	double epoch = atof(argv[4]); //struct timeval epoch?
	epoch = epoch / 1000;

	unsigned int no_obs_pkts = 0;
	unsigned int no_ipv4_pkts = 0;
	unsigned int no_valid_pkts = 0;
	unsigned int ip_payload_size = 0;
	unsigned int no_tcp_pkts = 0;
	unsigned int no_udp_pkts = 0;
	unsigned int no_icmp_pkts = 0;

	pcap_t* pcap;
	char errbuf[256];
	struct pcap_pkthdr hdr;
	const u_char* pkt;					// raw packet
	double pkt_ts;						// raw packet timestamp
	double start_ts;

	struct ip* ip_hdr = NULL;
	struct tcphdr* tcp_hdr = NULL;

	unsigned int src_ip;
	unsigned int dst_ip;
	unsigned short src_port;
	unsigned short dst_port;

	Element** tables[2];
	unsigned int current_epoch = 0;
	int i, j;
	for(i = 0; i < 2; i++) {
		tables[i] = (Element**) malloc(sizeof(Element *) * 10000);
		for(j = 0; j < 10000; j++) {
			tables[i][j] = NULL;
		}
	}


	// open input pcap file                                         
	if ((pcap = pcap_open_offline("trace.pcap", errbuf)) == NULL) {
		fprintf(stderr, "ERR: cannot open %s (%s)\n", "trace.pcap", errbuf);
		exit(-1);
	}

	while ((pkt = pcap_next(pcap, &hdr)) != NULL) {
			// get the timestamp

			pkt_ts = (double)hdr.ts.tv_usec / 1000000 + hdr.ts.tv_sec;
			if(no_obs_pkts == 0) {
				start_ts = pkt_ts;
			}

			ip_hdr = (struct ip*)pkt;
			
			//get stat
			no_obs_pkts += 1;
			if((int)ip_hdr->ip_v!=4){
				continue;
			}
			no_ipv4_pkts += 1;

			if(ip_hdr->ip_sum == checksum((unsigned char*)ip_hdr)){
				no_valid_pkts += 1;
			}

			ip_payload_size = ip_hdr->ip_len - (ip_hdr->ip_hl << 2);

			// IP addresses are in network-byte order	
			src_ip = ip_hdr->ip_src.s_addr;
			dst_ip = ip_hdr->ip_dst.s_addr;
			if( (unsigned int)((pkt_ts - start_ts)/epoch) > current_epoch) {
				current_epoch = (unsigned int)((pkt_ts - start_ts)/epoch);
				clear_table(tables[current_epoch % 2]);
			}
			double current_byte_count = update(tables[current_epoch % 2], src_ip, ip_payload_size);
			print_ip(src_ip);
			print(": %.6lf MB\n", current_byte_count);
			if(current_byte_count > hh_thresh) {
				printf("Time %.6lf: Heavy hitter, \n", pkt_ts);
				print_ip(src_ip);
				print("\n");
			}
			if(current_epoch > 0) {
				unsigned int prev_count = query(tables[(current_epoch-1) % 2], src_ip);
				if(abs(current_byte_count - prev_count) > hc_thresh) {
					printf("Time %.6lf: Heavy changer, ", pkt_ts);
					print_ip(src_ip);
					print("\n");
				}
			}
			if (ip_hdr->ip_p == IPPROTO_TCP) {
				no_tcp_pkts += 1;
				// tcp_hdr = (struct tcphdr*)((u_char*)ip_hdr + 
				// 		(ip_hdr->ip_hl << 2)); 
				// src_port = ntohs(tcp_hdr->source);
				// dst_port = ntohs(tcp_hdr->dest);

				// printf("%lf: %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d\n", 
				// 		pkt_ts, 
				// 		src_ip & 0xff, (src_ip >> 8) & 0xff, 
				// 		(src_ip >> 16) & 0xff, (src_ip >> 24) & 0xff, 
				// 		src_port, 
				// 		dst_ip & 0xff, (dst_ip >> 8) & 0xff, 
				// 		(dst_ip >> 16) & 0xff, (dst_ip >> 24) & 0xff, 
				// 		dst_port);
			}
			else if (ip_hdr->ip_p == IPPROTO_UDP) {
				no_udp_pkts += 1;
			} 
			else if (ip_hdr->ip_p == IPPROTO_ICMP) {
				no_icmp_pkts += 1;
			}
		
	}
	//print stat
	printf("the total number of observed packets: %d\n",no_obs_pkts);
	printf("the total number of observed IPv4 packets: %d\n",no_ipv4_pkts);
	printf("the total number of valid IPv4 packets that pass the checksum test: %d\n",no_valid_pkts);
	printf("the total IP payload size(valid IPv4 packets only): %d\n",ip_payload_size);
	printf("the total number of TCP packets (valid IPv4 packets only): %d\n",no_tcp_pkts);
	printf("the total number of UDP packets (valid IPv4 packets only): %d\n",no_udp_pkts);
	printf("the total number of ICMP packets (valid IPv4 packets only): %d\n",no_icmp_pkts);
	// close files
	pcap_close(pcap);
	
	return 0;
}
