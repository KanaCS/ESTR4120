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
#include <string.h>

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

typedef struct ips{
	unsigned int ip;
	struct ips* next;
} Des_ips;

typedef struct element {
	struct element* next;
	unsigned int ip_addr;
	double cumu_byte_count; // in units of 10^6 Bytes
	int report_status[3]; // {HH, HC, SS}
	double last_pkt_ts;
	Des_ips *des_ip;
} Element;

Element* update(Element** table, unsigned int ip_addr, unsigned int payload_size, double ts, unsigned int dip) { // return element *
	unsigned int ind = ip_addr % 10000;
	if(table[ind] == NULL) {
		// add new entry
		table[ind] = (Element *) malloc(sizeof(Element));
		table[ind]->next = NULL;
		table[ind]->ip_addr = ip_addr;
		table[ind]->cumu_byte_count = payload_size/1.0E6;
		table[ind]->report_status[0] = 0; table[ind]->report_status[1] = 0; table[ind]->report_status[2] = 0;
		table[ind]->last_pkt_ts = ts;
		table[ind]->des_ip = (Des_ips *) malloc(sizeof(Des_ips));
		table[ind]->des_ip->ip = dip;
		table[ind]->des_ip->next = NULL;
		return table[ind];
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
				target->next->report_status[0] = 0; target->next->report_status[1] = 0; target->next->report_status[2] = 0;
				target->next->last_pkt_ts = ts;
				target->next->des_ip = (Des_ips *) malloc(sizeof(Des_ips));
				target->next->des_ip->ip = dip;
				target->next->des_ip->next = NULL;
				return target->next;
			}
		}
		// entry found, update byte count
		target->cumu_byte_count += payload_size/1.0E6;
		// update last packet timestamp
		target->last_pkt_ts = ts;
		//update des_ip list
		Des_ips* desip = target->des_ip;
		while(desip->ip != dip){
			if(desip->next != NULL){
				desip = desip->next;
			}
			else{
				//add new entry
				desip->next = (Des_ips *) malloc(sizeof(Des_ips));
				desip->next->ip = dip;
				desip->next->next = NULL;
			}
		}
		//entry found then do nothing
		return target;
	}
}

unsigned int count_dstip(Element** table, unsigned int ip_addr){
	unsigned int ind = ip_addr % 10000;
	Des_ips* target;
	target = table[ind]->des_ip;
	unsigned int count = 1;
	while(target->next != NULL){
		count ++;
		target = target->next;
	}
	return count;
}

Element* query(Element** table, unsigned int ip_addr) {
	unsigned int ind = ip_addr % 10000;
	if(table[ind] == NULL) {
		return NULL;
	}
	else {
		Element* target;
		target = table[ind];
		while(target->ip_addr != ip_addr) {
			if(target->next != NULL) {
				target = target->next;
			} 
			else {
				return NULL;
			}
		}
		return target;
	}
}

void free_helper(Element* t) {
	Element* head = t;
	while(head != NULL) {
		t = head->next;
		free(head);
		head = t;
	}
	// unsigned int length = 1;
	// Element* head = t;
	// while(t->next != NULL) {
	// 	length++;
	// 	t = t->next;
	// }
	// Element** table = (Element** )malloc(sizeof(Element* ) * length);
	// int i;
	// t = head;
	// for(i = length-1; i > 0; i--) {
	// 	table[i] = t;
	// 	t = t->next;
	// }
	// table[0] = t;
	// for(i = 0; i <length; i++) {
	// 	free(table[i]);
	// }
	// free(table);
}

void clear_table(Element** table) {
	int i;
	for(i = 0; i < 10000; i++) {
		if(table[i] != NULL) {
			free_helper(table[i]);
			table[i] = NULL;
		}
	}
}

void print_ip(unsigned int ip) {
	printf("%d.%d.%d.%d", ip & 0xFF, (ip>>(8*1)) & 0xFF, (ip>>(8*2)) & 0xFF, (ip>>(8*3)) & 0xFF);
}

double abs_double(double d) {
	if(d<0) {
		return -d;
	}
	else {
		return d;
	}
}

typedef struct reported_HC {
	struct reported_HC* next;
	unsigned int ip_addr;
} Reported_HC;

void free_helper_HC(Reported_HC* t) {
	Reported_HC* head = t;
	while(head != NULL) {
		t = head->next;
		free(head);
		head = t;
	}
}
int reported_as_HC(Reported_HC *head, unsigned int ip) {
	while(head != NULL) {
		if(head->ip_addr == ip) {
			return 1; // already reported
		}
		head = head->next;
	}
	return 0; // not yet report
}

Reported_HC* report_HC(double from_count, double to_count, unsigned int ip_addr, double ts, unsigned int epoch, Reported_HC *tail) {
	printf("Epoch %d ", epoch);
	printf("Time %.6lf: Heavy changer %.6lfMB -> %.6lfMB, ", ts, from_count, to_count);
	print_ip(ip_addr);
	printf("\n");
	tail->next = (Reported_HC *)malloc(sizeof(Reported_HC));
	tail->next->next = NULL;
	tail->next->ip_addr = ip_addr;
	return tail->next;
}

void check_HC(Element **old_table, Element **new_table, Reported_HC *reported, double hc_thresh, unsigned int current_epoch, double current_epoch_finish_ts, int type) {
	// type == 0 means that old_table is epoch-1, new_table is epoch
	// type == 1 means that old_table is epoch, new_table is epoch-1

	// possible cases:
	// 0 MB -> BIG MB
	// BIG MB -> 0 MB
	// delta > BIG MB
	int j = 0;
	Element *t=NULL;
	Element *t_prev=NULL;
	// find tail in reported
	Reported_HC *tail=reported;
	while(tail->next != NULL) {
		tail = tail->next;
	}
	for(j = 0; j < 10000; j++) {
		if(new_table[j] != NULL) {
			t = new_table[j];
			while(t != NULL && reported_as_HC(reported, t->ip_addr) == 0) {
				t_prev = query(old_table, t->ip_addr);
				if(t_prev == NULL) { // entry exist in new_table but not old_table, possible 0 MB -> BIG MB when type == 0 and BIG MB -> 0 MB when type == 1
					if(t->cumu_byte_count > hc_thresh) {
						if(type == 0) { // 0 MB -> BIG MB
							tail = report_HC(0.0, t->cumu_byte_count, t->ip_addr, t->last_pkt_ts, current_epoch, tail);
						}
						else { // BIG MB -> 0 MB 
							tail = report_HC(0.0, t->cumu_byte_count, t->ip_addr, current_epoch_finish_ts, current_epoch, tail);
						}
					}
				}
				else if(abs_double(t->cumu_byte_count - t_prev->cumu_byte_count) > hc_thresh) {
					if(type == 0) {
						tail = report_HC(t_prev->cumu_byte_count, t->cumu_byte_count, t->ip_addr, t->last_pkt_ts, current_epoch, tail);
					}
					else {
						tail = report_HC(t->cumu_byte_count, t_prev->cumu_byte_count, t->ip_addr, t->last_pkt_ts, current_epoch, tail);
					}
				}
				t = t->next;
			}
		}
	}
}

/***************************************************************************
 * Main program
 ***************************************************************************/
int main(int argc, char** argv) {
	if(argc != 6){
		fprintf(stderr, "Usage: ./myids <hh_thresh> <hc_thresh> <ss_thresh> <epoch> <pcap_file_path>");
		exit(1);
	}
	double hh_thresh = atof(argv[1]);
	double hc_thresh = atof(argv[2]);
	unsigned int ss_thresh = atoi(argv[3]);
	double epoch = atof(argv[4]); //struct timeval epoch?
	char filename[100];
    strcpy(filename, argv[5]);
	epoch = epoch / 1000;

	unsigned int no_obs_pkts = 0;
	unsigned int no_ipv4_pkts = 0;
	unsigned int no_valid_pkts = 0;
	unsigned int ip_payload_size = 0;
	unsigned int no_tcp_pkts = 0;
	unsigned int no_udp_pkts = 0;
	unsigned int no_icmp_pkts = 0;
	unsigned int total_payload_size = 0;
	pcap_t* pcap;
	char errbuf[256];
	struct pcap_pkthdr hdr;
	const u_char* pkt;					// raw packet
	double pkt_ts;						// raw packet timestamp
	double start_ts;

	struct ip* ip_hdr = NULL;

	unsigned int src_ip;
	unsigned int dst_ip;

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
	if ((pcap = pcap_open_offline(filename, errbuf)) == NULL) {
		fprintf(stderr, "ERR: cannot open %s (%s)\n", filename, errbuf);
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

			ip_payload_size = ntohs(ip_hdr->ip_len) - ((unsigned int)(ip_hdr->ip_hl) << 2);
			total_payload_size += ip_payload_size;
			// IP addresses are in network-byte order	
			src_ip = ip_hdr->ip_src.s_addr;
			dst_ip = ip_hdr->ip_dst.s_addr;
			if( (unsigned int)((pkt_ts - start_ts)/epoch) > current_epoch) {
				// start of new epoch = end of current_epoch
				// check heavy changer here
				if(current_epoch > 0) {
					Reported_HC *head = (Reported_HC *) malloc(sizeof(Reported_HC));
					head->next = NULL;
					head->ip_addr = 0;
					check_HC(tables[(current_epoch-1)%2], tables[current_epoch%2], head, hc_thresh, current_epoch, start_ts + (current_epoch+1)*epoch, 0);
					check_HC(tables[current_epoch%2], tables[(current_epoch-1)%2], head, hc_thresh, current_epoch, start_ts + (current_epoch+1)*epoch, 1);
					free_helper_HC(head);
				}
				current_epoch = (unsigned int)((pkt_ts - start_ts)/epoch);
				clear_table(tables[current_epoch % 2]);
			}

			Element* elm = update(tables[current_epoch % 2], src_ip, ip_payload_size, pkt_ts, dst_ip);
			// print_ip(src_ip);
			// printf(": %.6lf MB\n", elm->cumu_byte_count );
			if(elm->cumu_byte_count > hh_thresh && elm->report_status[0] == 0) {
				printf("Epoch %d ", current_epoch);
				printf("pkt %d ", no_obs_pkts);
				printf("Time %.6lf: Heavy hitter of %.6lfMB, ", pkt_ts, elm->cumu_byte_count);
				print_ip(src_ip);
				printf("\n");
				elm->report_status[0] = 1;
			}
			
			unsigned int no_des_ip = count_dstip(tables[current_epoch % 2], src_ip);
			if(no_des_ip > ss_thresh){
				printf("Epoch %d ", current_epoch);
				printf("pkt %d ", no_obs_pkts);
				printf("Time %.6lf: Superspreader of %u, ", pkt_ts, no_des_ip);
				print_ip(src_ip);
				printf("\n");
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
	printf("the total IP payload size(valid IPv4 packets only): %d\n",total_payload_size);
	printf("the total number of TCP packets (valid IPv4 packets only): %d\n",no_tcp_pkts);
	printf("the total number of UDP packets (valid IPv4 packets only): %d\n",no_udp_pkts);
	printf("the total number of ICMP packets (valid IPv4 packets only): %d\n",no_icmp_pkts);
	// debug stat
	printf("pcap duration: %.6lf s\n", pkt_ts-start_ts);
	// close files
	pcap_close(pcap);
	
	return 0;
}
