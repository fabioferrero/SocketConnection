#include <stdio.h> 		
#include <stdlib.h>
#include <string.h>

#include "conn.h"

#define OK "+OK\r\n"
#define ERROR "-ERR\r\n"
#define TOKEN 65536
#define MAX_FILENAME 51

int main(int argc, char *argv[]) {

	Connection conn;
	char filename[MAX_FILENAME], * request;
	int str_len;
	
	if (argc != 3) {
		fprintf(stderr, "syntax: %s <address> <port>\n", argv[0]); 
		exit(-1);
	}
	
	checkaddress(argv[1]);
	
	conn = conn_connect(argv[1], checkport(argv[2]));
	
	printf("Insert filename or <quit> to close the connection\n");
	
	while(1) {
		printf("> ");
		str_len = readline(filename, MAX_FILENAME);
		
		if (!strcmp(filename, "QUIT") || !strcmp(filename, "quit")) {
			printf("Terminating connection with server...\n");
			close(conn.id);
			break;
		}
		printf("\trequested file: %s\n", filename);
		
		request = malloc((str_len+6)*sizeof(*request));
		
		sprintf(request, "GET %s\r\n", filename);
		
		//TODO modify connection function using Connection instead Host
		//conn_sends(, request);
		
	}
	
	return 0;
}


