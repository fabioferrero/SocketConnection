#include "conn.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_FILENAME 51

/* Number of active processes */
int activeProc = 0;

pthread_mutex_t mutex;
pthread_cond_t cond;

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

			//printf("\tGET request for file: %s\n", filename);

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

			//printf("\tSELECTED FILE: %s\n\t\tFile size: [%d B]\n\t\tLast mod: %s", filename, file_dim, ctime(&file_info.st_mtime));
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

void manageChild(int sig) {

	if (sig == SIGCHLD) {
		/* One of the children is died, but can be that more than one are died */
		pthread_mutex_lock(&mutex);
		while (waitpid(-1, NULL, WNOHANG) > 0) {
			activeProc--;
		}
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
}

int main(int argc, char *argv[]) {

	int maxServerProcesses = 2;

	Connection conn;
	Host local;
	int port;

	if (argc != 3) {
		if (argc != 2) {
			fprintf(stderr, "syntax: %s <port> [nproc]\n", argv[0]);
			exit(-1);
		}
	} else {
		port = atoi(argv[2]);
		if (port <= 0)
			fprintf(stderr, "Wrong number of processes: set default [%d].\n", maxServerProcesses);
		else
			maxServerProcesses = port;
	}

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);

	signal(SIGCHLD, manageChild);

	port = atoi(argv[1]);

	local = prepareServer(port, TCP);

	while(1) {
	
		pthread_mutex_lock(&mutex);
		while (activeProc == maxServerProcesses) {
			pthread_cond_wait(&cond, &mutex);
		}
		activeProc++;
		pthread_mutex_unlock(&mutex);
		
		printf("Waiting a new connection...\n");
		conn = acceptConn(&local);
		if (conn.id == -1) continue;

		if (!fork()) {
			/* Child process close the server connection */
			closeServer(local);
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
