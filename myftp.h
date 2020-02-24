struct message_s { //in total 10 bytes
    unsigned char protocol[5];  // 5 bytes
    unsigned char type;         // 1 bytes
    unsigned int length;        // header + payload, 4 bytes
} __attribute__ ((packed));

int sendn(int sd, void *buf, int buf_len);

int recvn(int sd, void *buf, int buf_len);