#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

void *sys_exec() {
    system("./client 127.0.0.1 1111 list");
}

int main(int argc, char **argv) {
	pthread_t thread[10];
	int i=0;
	for (i = 0; i < 10; i++) {
	int ret_val = pthread_create(&thread[i], NULL, sys_exec, NULL);
	}
	printf("This is master thread\n");
	for (i = 0; i < 10; i++)
	pthread_join(thread[i], NULL);
	return 0;
}