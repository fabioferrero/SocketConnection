#include "types.h"
#include "conn.h"

#include <sys/types.h>
#include <sys/wait.h>

#define MAX_FILENAME 51
#define BUFSIZE 4096

/* Serve the connection with XDR */
void serveConnXDR(Connection conn) {

	int bytes, sended_bytes, fd, count;
	uint32_t file_dim, timestamp;
	uint32_t file_dim_net, timestamp_net;
	float percent, seconds, bytes_per_sec;

	time_t start_send, end_send;

	char request[MAX_FILENAME+10], op[10], *response;
	char filename[MAX_FILENAME], path[MAX_FILENAME+6] = "files/";

	struct stat file_info;
	
	XDR xdrsIn, xdrsOut;
	char send_buf[BUFSIZE], recv_buf[BUFSIZE];
	file * file_xdr;
	message * msg;
	
	xdrmem_create(&xdrsOut, send_buf, BUFSIZE, XDR_ENCODE);
	xdrmem_create(&xdrsIn, recv_buf, BUFSIZE, XDR_DECODE);
	msg = malloc(sizeof(*msg));
	
	while(1) {
		printf("\tWaiting file request..\n");
		bytes = conn_recvn(conn, recv_buf, sizeof(message));
		if (bytes <= 0) 
			break;

		xdr_message(&xdrsIn, msg);
		
		switch (msg->tag) {
			case QUIT:
				printf("\tQUIT request\n");
				break;
			default: 
				printf("\tINVALID request\n");
				bytes = conn_sends(conn, ERROR);
				if (bytes <= 0)
					break;
				break;
		}

		/*if (strcmp(op, "GET") == 0) {
			strcpy(filename, request+4);

			strcpy(path, "files/");
			strcat(path, filename);

			fd = open(path, O_RDONLY);
			if (fd == -1) {
				report_err("Cannot open the file");
				bytes = conn_sends(conn, ERROR);
				break;
			}

			if (fstat(fd, &file_info)) {
				report_err("Cannot retreive file information");
				continue;
			}

			file_dim = file_info.st_size;
			timestamp = file_info.st_mtime;

			printf("\tSELECTED FILE: %s\n\t\tFile size:\t[%d B]\n\t\tTimestamp:\t[%d]\n", filename, file_dim, timestamp);

			// Send the header
			file_dim_net = htonl(file_dim);
			timestamp_net = htonl(timestamp);

			bytes = conn_sends(conn, OK);
			if (bytes <= 0)
				break;
			bytes = conn_sendn(conn, &file_dim_net, sizeof(file_dim));
			if (bytes <= 0)
				break;
			bytes = conn_sendn(conn, &timestamp_net, sizeof(timestamp));
			if (bytes <= 0)
				break;

			// Send the file
			time(&start_send);
			if (file_dim <= TOKEN) {
				response = malloc(file_dim*sizeof(*response));
				if (response == NULL) {
					report_err("Cannot allocate memory for response");
					continue;
				}
				bytes = read(fd, response, file_dim);
				if (bytes == -1) {
					report_err("Cannot read file");
				}
				bytes = conn_sendn(conn, response, file_dim);
				if (bytes <= 0)
					break;
			} else {
				response = malloc((TOKEN)*sizeof(*response));
				if (response == NULL) {
					report_err("Cannot allocate memory for response");
					continue;
				}
				sended_bytes = 0;
				count = 0;
				while (sended_bytes != file_dim) {
					bytes = read(fd, response, TOKEN);
					if (bytes == -1) {
						report_err("Cannot read file");
					}
					bytes = conn_sendn(conn, response, bytes);
					if (bytes <= 0)
						break;
					sended_bytes += bytes;
					count++;
					if ((count % 5) == 0) {
						percent = (float) sended_bytes / (float) file_dim * 100;
						printf("\r\t\tSending... [%.0f %%]", percent);
					}
				}
				printf("\r\t\t[100 %%] ");
			}
			time(&end_send);

			seconds = difftime(end_send, start_send);

			bytes_per_sec = (float) file_dim / seconds;

			if (seconds > 0)
				if (bytes_per_sec <= 1000)
					printf("File sended in %.2f seconds. [%.2f B/s]\n", seconds, bytes_per_sec);
				else if (bytes_per_sec < 1000000)
					printf("File sended in %.2f seconds. [%.2f KB/s]\n", seconds, bytes_per_sec / 1000);
				else
					printf("File sended in %.2f seconds. [%.2f MB/s]\n", seconds, bytes_per_sec / 1000000);
			else
				printf("File sended.\n");

			free(response);
		}*/
	}
}

/* Serve the connection */
void serveConn(Connection conn) {

	int bytes, sended_bytes, fd, count;
	uint32_t file_dim, timestamp;
	uint32_t file_dim_net, timestamp_net;
	float percent, seconds, bytes_per_sec;

	time_t start_send, end_send;

	char request[MAX_FILENAME+10], op[10], *response;
	char filename[MAX_FILENAME], path[MAX_FILENAME+6] = "files/";

	struct stat file_info;
	
	while(1) {
		printf("\tWaiting file request..\n");
		bytes = conn_recvs(conn, request, sizeof(request), "\r\n");
		if (bytes <= 0) break;

		sscanf(request, "%s", op);

		if (strcmp(op, "GET") == 0) {
			strcpy(filename, request+4);

			strcpy(path, "files/");
			strcat(path, filename);

			fd = open(path, O_RDONLY);
			if (fd == -1) {
				report_err("Cannot open the file");
				bytes = conn_sends(conn, ERROR);
				break;
			}

			if (fstat(fd, &file_info)) {
				report_err("Cannot retreive file information");
				continue;
			}

			file_dim = file_info.st_size;
			timestamp = file_info.st_mtime;

			printf("\tSELECTED FILE: %s\n\t\tFile size:\t[%d B]\n\t\tTimestamp:\t[%d]\n", filename, file_dim, timestamp);

			/* Send the header */
			file_dim_net = htonl(file_dim);
			timestamp_net = htonl(timestamp);

			bytes = conn_sends(conn, OK);
			if (bytes <= 0)
				break;
			bytes = conn_sendn(conn, &file_dim_net, sizeof(file_dim));
			if (bytes <= 0)
				break;
			bytes = conn_sendn(conn, &timestamp_net, sizeof(timestamp));
			if (bytes <= 0)
				break;

			/* Send the file */
			time(&start_send);
			if (file_dim <= TOKEN) {
				response = malloc(file_dim*sizeof(*response));
				if (response == NULL) {
					report_err("Cannot allocate memory for response");
					continue;
				}
				bytes = read(fd, response, file_dim);
				if (bytes == -1) {
					report_err("Cannot read file");
				}
				bytes = conn_sendn(conn, response, file_dim);
				if (bytes <= 0)
					break;
			} else {
				response = malloc((TOKEN)*sizeof(*response));
				if (response == NULL) {
					report_err("Cannot allocate memory for response");
					continue;
				}
				sended_bytes = 0;
				count = 0;
				while (sended_bytes != file_dim) {
					bytes = read(fd, response, TOKEN);
					if (bytes == -1) {
						report_err("Cannot read file");
					}
					bytes = conn_sendn(conn, response, bytes);
					if (bytes <= 0)
						break;
					sended_bytes += bytes;
					count++;
					if ((count % 5) == 0) {
						percent = (float) sended_bytes / (float) file_dim * 100;
						printf("\r\t\tSending... [%.0f %%]", percent);
					}
				}
				printf("\r\t\t[100 %%] ");
			}
			time(&end_send);

			seconds = difftime(end_send, start_send);

			bytes_per_sec = (float) file_dim / seconds;

			if (seconds > 0)
				if (bytes_per_sec <= 1000)
					printf("File sended in %.2f seconds. [%.2f B/s]\n", seconds, bytes_per_sec);
				else if (bytes_per_sec < 1000000)
					printf("File sended in %.2f seconds. [%.2f KB/s]\n", seconds, bytes_per_sec / 1000);
				else
					printf("File sended in %.2f seconds. [%.2f MB/s]\n", seconds, bytes_per_sec / 1000000);
			else
				printf("File sended.\n");

			free(response);

		} else if (strcmp(op, "QUIT") == 0) {
			printf("\tQUIT request\n");
			//conn_close(remote);
			break;
		} else {
			printf("\tINVALID request\n");
			bytes = conn_sends(conn, ERROR);
			if (bytes <= 0)
				break;
		}
	}
}

int main(int argc, char *argv[]) {

	int maxServerProcesses = 2;

	Connection conn;
	Host local;
	int port, userNproc, numberOfArgs, count = 0;
	
	/* Flag for XDR */
	int XDRenabled = 0;
	
	numberOfArgs = argc;
	
	if (!strcmp(argv[1], "-x")) {
		XDRenabled = 1;
		printf("-- XDR coding ENABLED --\n");
		numberOfArgs--;
	}
	
	if (numberOfArgs != 3) {
		if (numberOfArgs != 2) {
			fprintf(stderr, "syntax: %s [-x] <port> [nproc]\n", argv[0]);
			exit(-1);
		} else {
			/* Make sure that the port is the last arguments */
			numberOfArgs++; 
		}
	} else {
		userNproc = atoi(argv[numberOfArgs]);
		if (userNproc <= 0)
			fprintf(stderr, "Wrong number of processes: set default [%d].\n", maxServerProcesses);
		else
			maxServerProcesses = userNproc;
	}

	port = checkport(argv[numberOfArgs-1]);

	local = prepareServer(port, TCP);

	while(1) {
		if (count < maxServerProcesses) {
			count++;
			printf("Waiting a new connection...\n");
			conn = acceptConn(&local);
			if (conn.id == -1) continue;
	
			if (!fork()) {
				/* Close the passive connection *//* TODO implement in library */
				close(local.conn);
				/* Serve the new connection, than terminate */
				if (XDRenabled)
					serveConnXDR(conn);
				else
					serveConn(conn);
				conn_close(conn);
				return 0;
			} else {
				/* Main process, keep going listen */
				conn_close(conn);
			}
		} else {
			wait(NULL);
			count--;
		}
	}

	return 0;
}
