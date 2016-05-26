#include "conn.h"

#define MAX_FILENAME 51

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

			printf("\tGET request for file: %s\n", filename);

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

			printf("SELECTED FILE: %s [%d B]\nLast mod: %s", filename, file_dim, ctime(&file_info.st_mtime));

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
						printf("\rSending... [%.0f %%]", percent);
					}
				}
				printf("\r[100 %%] ");
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

	Connection conn;
	Host local;
	int port;

	if (argc != 2) {
		fprintf(stderr, "syntax: %s <port>\n", argv[0]);
		exit(-1);
	}

	port = atoi(argv[1]);

	local = prepareServer(port, TCP);

	while(1) {
		printf("Waiting a new connection...\n");
		conn = acceptConn(&local);
		if (conn.id == -1) continue;
		
		if (!fork()) {
			/* Serve the new connection, than terminate */
			serveConn(conn);
			conn_close(conn);
			return 0;
		} else {
			/* Main process, keep going listen */
			conn_close(conn);
		}
	}

	return 0;
}
