#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // "struct sockaddr_in"
#include <arpa/inet.h>  // "in_addr_t"
int main() {
    // printf() displays the string inside quotation
    unsigned long long block_size=0;
    int n=0, k=0, lines=0;
    in_addr_t *ip;
    unsigned short *port;
    char tmp[20], tmp_ip[20], tmp_port[20];
   	FILE *fp = fopen("clientconfig.txt", "r");
	if(fp==NULL){
			perror("clientconfig.txt is not found");
			exit(1);
	}
    while( EOF != fscanf(fp, "%[^\n]\n", tmp) ){
        lines += 1;
    }

    if(lines-3 <= 0){
        perror("no available server is found");
        exit(1);
    }
    ip = (in_addr_t*)malloc((lines-3)); port = (unsigned short*)malloc(sizeof(unsigned short)*(lines-3));
	fseek(fp, 0, SEEK_SET);

    fscanf(fp, "%[^\n]\n", tmp); n = atoll(tmp); 
    fscanf(fp, "%[^\n]\n", tmp); k = atoll(tmp);
    fscanf(fp, "%[^\n]\n", tmp); block_size = atoll(tmp); 
    //printf("n:%d, k:%d, bs:%llu\n",n,k,block_size);

    unsigned long long i = 0;
    while (EOF != fscanf(fp, "%[^:]", tmp_ip) && fread(tmp, 1, 1, fp)!=0 && EOF != fscanf(fp, "%[^\n]\n", tmp_port))
    {
        i++;
        ip[i] = inet_addr(tmp_ip);
        port[i] = atoi(tmp_port); 
        printf(">%s %d\n",tmp_ip, port[i]);
    }
    printf("[%d]\n",sizeof(ip));
    fclose(fp);
    free(ip);
    free(port);
    return 0;
}