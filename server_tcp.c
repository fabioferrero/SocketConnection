#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

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

void signalHandler(int sig) {

	if (sig == SIGCHLD) {
		/* One of the children is died, but can be that more than one are died */
		pthread_mutex_lock(&mutex);
		while (waitpid(-1, NULL, WNOHANG) > 0) {
			activeProc--;
		}
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
	if (sig == SIGINT) {
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
		/* Some termination before die */
		
		printf("\n");
		exit(0);
	}
}

int main(int argc, char *argv[]) {

	int maxServerProcesses = 2;

	Connection conn;
	Host thisServer;
	int port, servNumber, ctrl;

	if (argc != 3) {
		if (argc != 2) {
			fprintf(stderr, "syntax: %s <port> [nproc]\n", argv[0]);
			exit(-1);
		}
	} else {
		servNumber = atoi(argv[2]);
		if (port <= 0)
			fprintf(stderr, "Wrong number of processes: set default [%d].\n", maxServerProcesses);
		else
			maxServerProcesses = servNumber;
	}

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);

	signal(SIGCHLD, signalHandler);
	signal(SIGINT, signalHandler);

	port = checkport(argv[1]);
	if (port == -1) return -1;

	thisServer = prepareServer(port, TCP);		// Dies on failure

	while(1) {
	
		pthread_mutex_lock(&mutex);
		while (activeProc == maxServerProcesses) {
			pthread_cond_wait(&cond, &mutex);
		}
		activeProc++;
		pthread_mutex_unlock(&mutex);
		
		printf("Waiting a new connection...\n");
		conn = acceptConn(thisServer);
		if (conn.id == -1) continue;

		if (!fork()) {
			/* Child process close the server connection */
			closeHost(thisServer);
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
