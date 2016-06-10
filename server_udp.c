#include "conn.h"

int main(int argc, char *argv[]) {

	Host remote;
	char data[DATAGRAM_LEN];
	int ctrl, port;
	
	if (argc != 2) {
		fprintf(stderr, "syntax: %s <port>\n", argv[0]); 
		exit(-1);
	}
	
	port = checkport(argv[1]);
	if (port == -1) return -1;
	
	/* Some possible initialization */
	remote = prepareServer(port, UDP);
		
	while(1) {
		printf("Waiting for datagram... [Max size: %dB]\n", DATAGRAM_LEN);
		
		/* I want to receive a string from a remote host and I want know
		   its address and port */
		ctrl = recvsfromHost(data, DATAGRAM_LEN, &remote, NO_TIMEOUT);
		if (ctrl == -1) {
			continue;
		}
		
		printf("Received [%s] from %s:%d\n", data, remote.address, remote.port);
		/* I want to send a string to the same host before */
		ctrl = sendstoHost(data, remote);
		if (ctrl == -1) {
			continue;
		}
	}
	
	/* Some free function here */
	ctrl = closeHost(remote);
	
	return 0;
}
