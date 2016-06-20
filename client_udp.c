#include "conn.h"

#define MAX_RETRY 5
#define TIME_TO_WAIT 3
#define MAX_LENGHT 100

int main(int argc, char *argv[]) {

	Host server;
	char name[MAX_LENGHT], response[MAX_LENGHT];
	int ctrl, port, retry;
	
	if (argc != 4) {
		fprintf(stderr, "syntax: %s <address> <port> <string>\n", argv[0]); 
		exit(-1);
	}
	
	//if (checkaddress(argv[1])) return -1;
	
	port = checkport(argv[2]);
	if (port == -1) return -1;
	
	/* Some possible initialization */
	server = Host_init(argv[1], port);
	
	strcpy(name, argv[3]);
	
	/* I want to send a string to a remote host */
	/* I have the address and port of the host */
	ctrl = sendstoHost(name, server);
	if (ctrl == -1) return -1;
	
	for (retry = 0; retry < MAX_RETRY; retry++) {
		/* I want to receive a string from a remote host */
		/* I want to know the address if i don't have already */
		ctrl = recvsfromHost(response, MAX_LENGHT, &server, TIME_TO_WAIT);
		if (ctrl == -1) {
			printf("Retry [%d on %d]\n", retry+1, MAX_RETRY);
		} else break;
	}
	
	if (ctrl == 0) {
		printf("[%s] from %s:%d\n", response, server.address, server.port);
	} else {
		fprintf(stderr, "Client terminates after %d retry.\n", MAX_RETRY);
	}
	
	/* Some free function here */
	ctrl = closeHost(server);
	
	return 0;
}


