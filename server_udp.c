#include <stdio.h> 		
#include <stdlib.h>
#include <string.h>

#include "conn.h"

int main(int argc, char *argv[]) {

	Host local, remote;
	char data[DATAGRAM_LEN];
	int bytes;
	
	if (argc != 2) {
		fprintf(stderr, "syntax: %s <port>\n", argv[0]); 
		exit(-1);
	}
	
	local = prepareServer(atoi(argv[1]), UDP);
	printf("Server ready\n");
		
	remote.conn = local.conn;
		
	while(1) {
		printf("Waiting for datagram... [Max size: %lu]\n", sizeof(data));
		
		bytes = recvsfromHost(data, &remote, 0);
		
		if (bytes != -1) {
			printf("Received [%s] from %s:%d\n", data, remote.address, remote.port);
			
			sendstoHost(data, &remote);
			
		}
	}
	
	return 0;
}
