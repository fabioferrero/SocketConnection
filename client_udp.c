#include <stdio.h> 		
#include <stdlib.h>
#include <string.h>

#include "conn.h"

#define MAX_RETRY 5
#define TIME_TO_WAIT 3

int main(int argc, char *argv[]) {

	Host remote;
	char name[31], response[31];
	int bytes, retry;
	
	if (argc != 4) {
		fprintf(stderr, "syntax: %s <address> <port> <string>\n", argv[0]); 
		exit(-1);
	}
	
	remote = Host_init(argv[1], atoi(argv[2]), UDP);
	
	strcpy(name, argv[3]);
	
	/* */
	sendstoHost(name, &remote);
	
	for (retry = 0; retry < MAX_RETRY; retry++) {
		bytes = recvsfromHost(response, &remote, TIME_TO_WAIT);
		if (bytes <= 0) {
			printf("Retry [%d on %d]\n", retry+1, MAX_RETRY);
		} else break;
	}
	
	if (bytes > 0) {
		printf("[%s] from %s:%d\n", response, remote.address, remote.port);
	} else {
		fprintf(stderr, "Client terminates after %d retry.\n", MAX_RETRY);
	}
	
	conn_close(remote.conn);
	
	return 0;
}


