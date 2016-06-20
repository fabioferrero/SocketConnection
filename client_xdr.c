#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

int requestFile(Connection conn, char * filename) {

	char * request;
	char header[6];
	int str_len = strlen(filename), fd, ctrl;
	
	uint32_t file_dim = 0, timestamp = 0;
	uint32_t file_dim_net = 0, timestamp_net = 0;
	
	XDR xdrsIn;
	FILE * streamIn;
	
	printf("\trequested file: %s\n", filename);

	request = malloc((str_len+6)*sizeof(*request));

	sprintf(request, "GET %s\r\n", filename);

	ctrl = conn_sends(conn, request);
	
	free(request);
	if (ctrl == -1) return -1;

	ctrl = conn_recvn(conn, header, 1);
	if (ctrl == -1) return -1;

	if (*header == '+') {
		ctrl = conn_recvs(conn, header+1, 4, "\r\n");
		if (ctrl == -1) return -1;
		ctrl = conn_recvn(conn, &file_dim_net, sizeof(file_dim));
		if (ctrl == -1) return -1;
		ctrl = conn_recvn(conn, &timestamp_net, sizeof(timestamp));
		if (ctrl == -1) return -1;
	
		file_dim = ntohl(file_dim_net);
		timestamp = ntohl(timestamp_net);
	
		printf("Received file dim: %u B\n", file_dim);
		printf("Received timestamp: %u\n", timestamp);
	
		fd = open(filename, O_RDWR | O_CREAT, 0644);
		if (fd == -1) {
			report_err("Cannot open the file");
			return -1;
		}
	
		/* Receive the file */
		
		streamIn = fdopen(dup(conn.sock), "r");
		xdrstdio_create(&xdrsIn, streamIn, XDR_DECODE);
		
		ctrl = xdr_recvfile(&xdrsIn, fd, file_dim);
		if (ctrl == -1)
			return -1;
		
		xdr_destroy(&xdrsIn);
		fclose(streamIn);
	
	} else if (*header == '-') {
		ctrl = conn_recvs(conn, header+1, 5, "\r\n");
		if (ctrl == -1) return -1;
		printf("Server response: [%s]\nTerminate.\n", header);
		return -1;
	} else {
		fprintf(stderr, "Response not recognized, terminate.\n");
		return -1;
	}
	
	close(fd);
	
	return 0;
}

void signalHandler(int sig) {

	if (sig == SIGCHLD) {
		/* One of the children is died, but can be that more than one are died */
		while (waitpid(-1, NULL, WNOHANG) > 0) {}
	}
	if (sig == SIGINT) {
		/* Some termination before die */
		
		printf("\n");
		exit(0);
	}
}

int main(int argc, char *argv[]) {

	Connection conn;
	char filename[MAX_FILENAME];
	int ctrl, first = TRUE;
	pid_t child;

	if (argc != 3) {
		fprintf(stderr, "syntax: %s <address> <port>\n", argv[0]);
		exit(-1);
	}

	if (checkaddress(argv[1])) 
		return -1;

	conn = conn_connect(argv[1], checkport(argv[2]));
	
	signal(SIGCHLD, signalHandler);
	signal(SIGINT, signalHandler);
	
	printf("Insert filename or <quit> to close the connection\n");
	
	while (TRUE) {
		
		readline(filename, MAX_FILENAME);

		if (!strcmp(filename, "Q") || !strcmp(filename, "q")) {
			printf("Terminating connection with server...\n");
			ctrl = conn_sends(conn, "QUIT\r\n");
			if (ctrl == -1) return -1;
			ctrl = conn_close(conn);
			if (ctrl == -1) return -1;
			return 0;
		}
		
		if (!strcmp(filename, "A") || !strcmp(filename, "a")) {
			printf("Aborting connection with server...\n");
			if (kill(child, SIGKILL))
				report_err("Cannot terminate a child");
			ctrl = conn_close(conn);
			if (ctrl == -1) return -1;
			return 0;
		}
		if (!first)
			wait(NULL);
		else 
			first = FALSE;
		
		child = fork();
		
		if (!child) {
			ctrl = requestFile(conn, filename);
			if (ctrl == -1) return -1; 
			return 0;
		}
	}

	return 0;
}
