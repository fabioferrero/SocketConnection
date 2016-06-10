#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

int main(int argc, char *argv[]) {

	Connection conn;
	char filename[MAX_FILENAME], * request, * file;
	char header[6];
	int str_len = 0, fd;
	
	uint32_t file_dim = 0, timestamp = 0;
	uint32_t file_dim_net = 0, timestamp_net = 0;

	if (argc != 3) {
		fprintf(stderr, "syntax: %s <address> <port>\n", argv[0]);
		exit(-1);
	}

	if (checkaddress(argv[1])) return -1;

	conn = conn_connect(argv[1], checkport(argv[2]));

	printf("Insert filename or <quit> to close the connection\n");

	while(1) {
		printf("> ");
		str_len = readline(filename, MAX_FILENAME);

		if (!strcmp(filename, "QUIT") || !strcmp(filename, "quit")) {
			printf("Terminating connection with server...\n");
			conn_sends(conn, "QUIT\r\n");
			conn_close(conn);
			break;
		}
		printf("\trequested file: %s\n", filename);

		request = malloc((str_len+6)*sizeof(*request));

		sprintf(request, "GET %s\r\n", filename);
		
		conn_sends(conn, request);
		if (conn_recvn(conn, header, 1) == -1) {
			break;
		}
		
		if (*header == '+') {
			conn_recvs(conn, header+1, 4, "\r\n");
		
			conn_recvn(conn, &file_dim_net, sizeof(file_dim));
			conn_recvn(conn, &timestamp_net, sizeof(timestamp));
			
			file_dim = ntohl(file_dim_net);
			timestamp = ntohl(timestamp_net);
			
			printf("Received file dim: %u B\n", file_dim);
			printf("Received timestamp: %u\n", timestamp);
			
			fd = open(filename, O_RDWR | O_CREAT);
			if (fd == -1) {
				report_err("Cannot open the file");
				break;
			}
			
			/* Receive the file */
			if (file_dim < TOKEN) {
				conn_recvfile(conn, fd, file_dim);
			} else {
				conn_recvfile_tokenized(conn, fd, file_dim, TOKEN);
			}	
			
		} else if (*header == '-') {
			conn_recvs(conn, header+1, 5, "\r\n");
			printf("Server response: [%s]\nTerminate.\n", header);
			free(request);
			return -1;
		} else {
			fprintf(stderr, "Response not recognized, terminate.\n.");
			free(request);
			return -1;
		}
		free(request);
		close(fd);
	}

	return 0;
}
