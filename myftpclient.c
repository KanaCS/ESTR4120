# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <arpa/inet.h>

# define PORT 12345

int main(int argc, char **argv) {
	int sd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server_addr;
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=inet_addr("137.189.88.152");
	server_addr.sin_port=htons(PORT);
    return 0;
}