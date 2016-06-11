#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

#define MAX_PROC 10

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
	
	bytes = conn_setTimeout(conn, 120);
	if (bytes == -1) {
		printf("Terminating service.\n");
		return;
	}

	while(1) {
		printf("\tWaiting file request..\n");
		bytes = conn_recvs(conn, request, sizeof(request), "\r\n");
		if (bytes == -1) break;

		sscanf(request, "%s", op);

		if (strcmp(op, "GET") == 0) {
			strcpy(filename, request+4);

			strcpy(path, "files/");
			strcat(path, filename);

			fd = open(path, O_RDONLY);
			if (fd == -1) {
				report_err("Cannot open the file");
				bytes = conn_sends(conn, "-ERR\r\n");
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

			bytes = conn_sends(conn, "+OK\r\n");
			if (bytes == -1)
				break;
			bytes = conn_sendn(conn, &file_dim_net, sizeof(file_dim));
			if (bytes == -1)
				break;
			bytes = conn_sendn(conn, &timestamp_net, sizeof(timestamp));
			if (bytes == -1)
				break;

			/* Send the file */
			
			if (file_dim <= TOKEN) {
				bytes = conn_sendfile(conn, fd, file_dim);
				if (bytes == -1)
					break;
			} else {
				bytes = conn_sendfile_tokenized(conn, fd, file_dim, TOKEN);
				if (bytes == -1)
					break;
			}

		} else if (strcmp(op, "QUIT") == 0) {
			printf("\tQUIT request\n");
			break;
		} else {
			printf("\tINVALID request\n");
			bytes = conn_sends(conn, "-ERR\r\n");
			if (bytes == -1)
				break;
		}
	}
	printf("Terminating service.\n");
}

int main(int argc, char *argv[]) {

	int serverProcesses = 2;

	Connection conn;
	Host thisServer;
	int port, servNumber, ctrl, i;
	pid_t * children;

	if (argc != 3) {
		if (argc != 2) {
			fprintf(stderr, "syntax: %s <port> [nproc]\n", argv[0]);
			exit(-1);
		}
	} else {
		servNumber = atoi(argv[2]);
		if (servNumber <= 0 || servNumber > MAX_PROC)
			fprintf(stderr, "Wrong number of processes: set default [%d].\n", serverProcesses);
		else
			serverProcesses = servNumber;
	}
	
	children = malloc(serverProcesses * sizeof(pid_t));
	if (children == NULL) {
		fprintf(stderr, "Cannot allocate memory fot children");
		return -1;
	}

	port = checkport(argv[1]);
	if (port == -1) return -1;

	thisServer = prepareServer(port, TCP);		// Dies on failure
	
	for (i = 0; i < servNumber; i++) {
		children[i] = fork();
		if (!children[i]) {
			/* Child */
			while(1) {
				printf("Waiting a new connection...\n");
				conn = acceptConn(thisServer);
				if (conn.id == -1) continue;

				serveConn(conn);
				conn_close(conn);
			}
		}
	}
	
	pause();
	
	/* Came here after a SIGINT */
	
	for (i = 0; i < servNumber; i++)
		if (kill(children[i], SIGKILL))
			report_err("Cannot terminate a child");
			
	printf("Lallallero\n");
	
	return 0;
}
