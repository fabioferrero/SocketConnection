#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "conn.h"

#define MAX_FILENAME 51
#define BUFSIZE 4096

void requestFilesXDR() {

}

void requestFiles() {

}

int main(int argc, char *argv[]) {

	Connection conn;
	char filename[MAX_FILENAME], * request, * file_data;
	char header[6];
	int str_len = 0, bytes = 0, fd, datareceived = 0, last_pack;
	
	/* Flag for XDR */
	int XDRenabled = 0;
	
	XDR xdrsIn, xdrsOut;
	char send_buf[BUFSIZE], recv_buf[BUFSIZE];
	file * file_xdr;
	message * msg;
	
	uint32_t file_dim = 0, timestamp = 0;
	uint32_t file_dim_net = 0, timestamp_net = 0;

	int numberOfArgs = argc;

	if (!strcmp(argv[1], "-x")) {
		XDRenabled = 1;
		printf("-- XDR coding ENABLED --\n");
		numberOfArgs--;
	}
	
	if (numberOfArgs != 3) {
		fprintf(stderr, "syntax: %s [-x] <address> <port>\n", argv[0]);
		exit(-1);
	} 
	
	checkaddress(argv[numberOfArgs-2]);
	conn = conn_connect(argv[numberOfArgs-2], checkport(argv[numberOfArgs-1]));
	
	/* Associate xdrs with buffers */
	xdrmem_create(&xdrsOut, send_buf, BUFSIZE, XDR_ENCODE);
	xdrmem_create(&xdrsIn, recv_buf, BUFSIZE, XDR_DECODE);
	msg = malloc(sizeof(*msg));

	printf("Insert filename or <quit> to close the connection\n");

	while(1) {
		printf("> ");
		str_len = readline(filename, MAX_FILENAME);

		if (!strcmp(filename, "QUIT") || !strcmp(filename, "quit")) {
			printf("Terminating connection with server...\n");
			
			if (XDRenabled) {
				msg->tag = QUIT;
				// Write the msg on the buffer
				if (!xdr_message(&xdrsOut, msg))
					printf("XDR error\n");
				// Send the message to the server
				conn_sendn(conn, send_buf, sizeof(message));
				printf("XDR sended\n");
			} else 
				conn_sends(conn, "QUIT\r\n");
			
			conn_close(conn);
			break;
		}
		
		printf("\trequested file: %s\n", filename);

		request = malloc((str_len+6)*sizeof(*request));

		sprintf(request, "GET %s\r\n", filename);
		
		if (XDRenabled) {
			msg->tag = GET;
			msg->filename = malloc(strlen()*sizeof(char))
			if (!xdr_message(&xdrsOut, msg))
				printf("XDR error\n");		
				
		} else 
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
				file_data = malloc(file_dim*sizeof(*file_data));
				if (file_data == NULL) {
					report_err("Cannot allocate memory for file");
					continue;
				}
				bytes = conn_recvn(conn, file_data, file_dim);
				bytes = writen(fd, file_data, bytes);
				if (bytes != file_dim) {
					fprintf(stderr, "Cannot write all the file\n");
					break;
				}
			} else {
				file_data = malloc((TOKEN)*sizeof(*file_data));
				if (file_data == NULL) {
					report_err("Cannot allocate memory for file");
					continue;
				}
				last_pack = file_dim % TOKEN;
				
				while(datareceived != file_dim - last_pack) {
					bytes = conn_recvn(conn, file_data, TOKEN);
					writen(fd, file_data, bytes);
					datareceived += bytes;
				}
				
				bytes = conn_recvn(conn, file_data, last_pack);
				writen(fd, file_data, bytes);
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
		free(file_data);
		close(fd);
	}

	return 0;
}
