struct message_s { //in total 10 bytes
    unsigned char protocol[5];  // 5 bytes
    unsigned char type;         // 1 bytes
    unsigned int length;        // header + payload, 4 bytes
} __attribute__ ((packed));

int sendn(int sd, void *buf, int buf_len);

int recvn(int sd, void *buf, int buf_len);

void print_message(struct message_s m) {
	printf("Message: %s, type: %#02X, length: %u\n", m.protocol, m.type, m.length);
}
#define BATCH_SIZE 2048 // 2KB
#define DPATH "data/"
#define DPATH_LEN 5
