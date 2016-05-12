#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conn.h"

#define MAX_FILENAME 51

int main(int argc, char *argv[]) {

	Connection conn;
	char filename[MAX_FILENAME], * request, * file;
	char header[6];
	int str_len = 0, bytes = 0, fd, datareceived = 0, last_pack;
	
	uint32_t file_dim = 0, timestamp = 0;
	uint32_t file_dim_net = 0, timestamp_net = 0;

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
			conn_sends(conn, "QUIT\r\n");
			conn_close(conn);
			break;
		}
		printf("\trequested file: %s\n", filename);

		request = malloc((str_len+6)*sizeof(*request));

		sprintf(request, "GET %s\r\n", filename);
		
		conn_sends(conn, request);
		conn_recvn(conn, header, 1);
		
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
			
			datareceived = 0;
			
			if (file_dim < TOKEN) {
				file = malloc(file_dim*sizeof(*file));
				if (file == NULL) {
					report_err("Cannot allocate memory for file");
					continue;
				}
				bytes = conn_recvn(conn, file, file_dim);
				bytes = writen(fd, file, bytes);
				if (bytes != file_dim) {
					fprintf(stderr, "Cannot write all the file\n");
					break;
				}
			} else {
				file = malloc((TOKEN)*sizeof(*file));
				if (file == NULL) {
					report_err("Cannot allocate memory for file");
					continue;
				}
				last_pack = file_dim % TOKEN;
				
				while(datareceived != file_dim - last_pack) {
					bytes = conn_recvn(conn, file, TOKEN);
					writen(fd, file, bytes);
					datareceived += bytes;
				}
				
				bytes = conn_recvn(conn, file, last_pack);
				writen(fd, file, bytes);
				datareceived += bytes;
				
				if (datareceived != file_dim) {
					fprintf(stderr, "Cannot write all the file\n");
					break;
				}
			}	
			printf("All file received: %s [%dB]\n", filename, file_dim);
		} else if (*header == '-') {
			bytes = conn_recvs(conn, header+1, 5, "\r\n");
			printf("Server response: [%s]\nTerminate.\n", header);
			free(request);
			return -1;
		} else {
			fprintf(stderr, "Response not recognized, terminate.\n.");
			free(request);
			return -1;
		}
		free(request);
		free(file);
		close(fd);
	}

	return 0;
}
