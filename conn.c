/*	conn.c 
	written by Fabio Ferrero
*/
#include "conn.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h> // Address conversion

/*
 	family: AF_INET	 for IPv4  type: SOCK_STREAM for TCP  protocol: IPPROTO_TCP
 			AF_INET6 for IPv6		 SOCK_DGRAM  for UDP 			IPPROTO_UDP

 	struct sockaddr_in {				htonl -> Host to Network long (address)
 		short sin_family;				htons -> Host to Network short (port)
 		u_short sin_port;				ntohl -> Network to Host long (address)
 		struct in_addr sin_addr;		ntohs -> Network to Host short (port)
 	}

 	struct in_addr {
 		unsigned long s_addr;
 	}

 	int   inet_aton(char *str, struct in_addr *addr); RETURN 1 ERROR 0
 	char* inet_ntoa(struct in_addr addr);
*/

#define SA struct sockaddr
#define QUEUE_SIZE 32

#define CORRECT(x) (x >= 0 && x < 256)
#define TIMEOUT_EXPIRED (TIMEOUT && (errno == EAGAIN || errno == EWOULDBLOCK))

uint32_t TIMEOUT = 0;

void fatal_err(char *message) {
	fprintf(stderr, "%s - (Error %d)\n", message, errno);
	perror("");
	exit(-1);
}

void report_err(char *message) {
	fprintf(stderr, "%s - (Error %d)\n", message, errno);
	perror("");
	return;
}

/* Returns an host used to acceptConn() on TCP or recvfromHost() on UDP */
Host prepareServer(int port, int protocol) {

	Host host;
	char prot[4];

	struct sockaddr_in addr;
	
	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}

	switch (protocol) {
		case TCP:
			host.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			strcpy(prot, "TCP");
			break;

		case UDP:
			host.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			strcpy(prot, "UDP");
			break;

		default:
			fprintf(stderr, "Invalid protocol\n");
			exit(-1);
	}
	if (host.sock == -1)
		fatal_err("Cannot set the server socket");
	else if (protocol == TCP)
		printf("Connection set on %s\n", prot);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (protocol == TCP) {
		host.port = port;
		strcpy(host.address, inet_ntoa(addr.sin_addr));
	}
	if (bind(host.sock, (SA*)&addr, sizeof(addr)))
		fatal_err("Cannot prepare the server");

	if (protocol == TCP) {
		printf("Server prepared [address %s:%d]\n", host.address, host.port);
		if (listen(host.sock, QUEUE_SIZE))
			fatal_err("Cannot listen on specified address");
	} else 
		printf("Enabled UDP on port %d.\n", port);
	
	return host;
}

int closeHost(Host host) {
	if (close(host.sock)) {
		report_err("Cannot close the socket");
		return -1;
	}
	return 0;
}

Connection acceptConn(Host server) {

	Connection conn;

	struct sockaddr_in addr;
	int addr_len = sizeof(addr);

	conn.id = accept(server.sock, (SA*)&addr, (socklen_t*)&addr_len);
	if (conn.id == -1) {
		report_err("Cannot accept a connection");
	} else {
		strcpy(conn.address, inet_ntoa(addr.sin_addr));
		conn.port = ntohs(addr.sin_port);
		printf("New connection from %s:%d\n", conn.address, conn.port);
	}

	return conn;
}

Host Host_init(char * address, int port) {

	Host h;

	if (checkaddress(address)) 
		exit(-1);
	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}
	
	strcpy(h.address, address);
	h.port = port;
		
	h.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (h.sock == -1)
		fatal_err("Cannot set the host socket");

	return h;
}

Connection conn_connect(char * address, int port) {
	
	Connection conn;
	int retval;
	struct sockaddr_in destination;
	
	if (checkaddress(address)) exit(-1);
	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}

	destination.sin_family = AF_INET;
	destination.sin_port = htons(port);
	inet_aton(address, &destination.sin_addr);
	
	conn.id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (conn.id == -1)
		fatal_err("Cannot create the connection");

	retval = connect(conn.id, (SA*)&destination, sizeof(destination));
	if (retval == -1)
		fatal_err("Cannot connect to endpoint");
	
	strcpy(conn.address, address);
	conn.port = port;
	printf("Connected to %s:%d\n", conn.address, conn.port);
	
	return conn;
}

int conn_close(Connection conn) {
	if (close(conn.id)) {
		report_err("Cannot close the connection");
		return -1;
	}
	return 0;
}

int conn_sends(Connection conn, char * string) {

	int datasended, dataremaining;
	char *ptr = string;

	dataremaining = strlen(string);

	while (dataremaining > 0) {
		datasended = send(conn.id, ptr, dataremaining, 0);
		if (datasended <= 0) { //error
			report_err("Cannot send string");
			return -1;
		} else {
			dataremaining -= datasended;
			ptr += datasended;
		}
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while sending data.\n");
		return -1; 
	}
	
	return 0;
}

int conn_sendn(Connection conn, void * data, int dataremaining) {

	int datasended;
	void *ptr = data;

	while (dataremaining > 0) {
		datasended = send(conn.id, ptr, dataremaining, 0);
		if (datasended <= 0) { //error
			report_err("Cannot send data");
			return -1;
		} else {
			dataremaining -= datasended;
			ptr += datasended;
		}
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while sending data.\n");
		return -1; 
	}
	
	return 0;
}

int conn_sendfile(Connection conn, int fd, int file_size) {
	
	int ctrl;
	char * data;
	
	data = malloc(file_size*sizeof(char));
	if (data == NULL) {
		report_err("Cannot allocate memory for data");
		return -1;
	}
	ctrl = readn(fd, data, file_size);
	if (ctrl == -1) {
		free(data);
		return -1;
	}
	ctrl = conn_sendn(conn, data, file_size);
	free(data);
	if (ctrl == -1) {
		return -1;
	}
	
	printf("All file sended. [%d B]\n", file_size);
	return 0;
}

int conn_sendfile_tokenized(Connection conn, int fd, int file_size, int tokenlen) {
	
	int ctrl, sended_bytes = 0, count = 0;
	int bytes, frequency = (file_size / tokenlen / 3000)+1;
	float percent;
	char * token;
	
	token = malloc(tokenlen*sizeof(char));
	if (token == NULL) {
		report_err("Cannot allocate memory for token");
		return -1;
	}
	
	while (sended_bytes < file_size) {
		bytes = read(fd, token, tokenlen);
		if (bytes == 0) 
			break;
		if (bytes == -1) {
			report_err("Cannot read the file");
			free(token);
			return -1;
		}
		ctrl == conn_sendn(conn, token, bytes);
		if (ctrl == -1) {
			free(token);
			return -1;
		}
		sended_bytes += bytes;
		count++;
		if ((count % frequency) == 0) {
			percent = (float) sended_bytes / (float) file_size * 100;
			printf("\r\tSending... [%.0f %%]", percent);
		}
	}
	free(token);
	
	if (sended_bytes != file_size) {
		fprintf(stderr, "Some error occurs while sending file.\n");
		return -1;
	} else { 
		printf("\r\tAll file sended. [100 %%]    \n");
		return 0;
	}
}

/* The function continue to wait data until the terminator sequence is reached
 * or the str_len is reached.
 * The string retrieved hasn't the terminator included.
 */
int conn_recvs(Connection conn, char * string, int str_len, char * terminator) {

	int i, datareceived, ter_len, counter = 0, equal = TRUE;
	char * ptr = string;

	ter_len = strlen(terminator);

	for (i=0; i< str_len; i++)
		string[i]='0';

	while (str_len > 0) {
		// Receive char by char
		datareceived = recv(conn.id, ptr, sizeof(char), 0);
		if (TIMEOUT_EXPIRED) {
			fprintf(stderr, "Timeout expired [%d sec]\n", TIMEOUT);
			return -1;
		}
		if (datareceived == 0) {
			printf("Connection closed by party\n");
			string[counter] = '\0';
			return -1;
		}
		if (datareceived == -1) {
			report_err("Cannot receive data");
			string[counter] = '\0';
			return -1;
		}
		counter += datareceived;
		if (terminator != NULL) {
			if (string[counter-ter_len] == terminator[0]) {
				// Check if the terminator is reached
				equal = TRUE;
				for(i=1; i<ter_len; i++) {
					if (string[counter-ter_len+i] != terminator[i]) {
						equal = FALSE;
						break;
					}
				}
				if (equal)
					break;
			}
		}
		ptr += datareceived;
		str_len -= datareceived;
	}

	string[counter-ter_len] = '\0';
	
	return 0;
}

/* The function continue to wait data until str_len data are received.
 */
int conn_recvn(Connection conn, void * data, int dataremaining) {

	int datareceived;
	void * ptr = data;

	while (dataremaining > 0) {
		datareceived = recv(conn.id, ptr, dataremaining, 0);
		if (TIMEOUT_EXPIRED) {
			fprintf(stderr, "Timeout expired [%d sec]\n", TIMEOUT);
			return -1;
		}
		if (datareceived == 0) {
			printf("Connection closed by party\n");
			return -1;
		}
		if (datareceived == -1) {
			report_err("Cannot receive data");
			return -1;
		}
		ptr += datareceived;
		dataremaining -= datareceived;
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while receiving data.\n");
		return -1;
	}
	
	return 0;
}

int conn_recvfile(Connection conn, int fd, int file_size) {

	int ctrl;
	char * data;
	
	data = malloc(file_size*sizeof(char));
	if (data == NULL) {
		report_err("Cannot allocate memory for data");
		return -1;
	}
	ctrl = conn_recvn(conn, data, file_size);
	if (ctrl == -1) {
		free(data);
		return -1;	
	}
	ctrl = writen(fd, data, file_size);
	free(data);
	if (ctrl == -1) {
		return -1;
	}
	
	printf("All file received. [%d B]\n", file_size);
	return 0;
}

int conn_recvfile_tokenized(Connection conn, int fd, int file_size, int tokenlen) {

	int ctrl, last_pack, datareceived = 0;
	char * token;

	token = malloc(tokenlen*sizeof(char));
	if (token == NULL) {
		report_err("Cannot allocate memory for token");
		return -1;
	}
	
	last_pack = file_size % tokenlen;
	
	while (datareceived != file_size - last_pack) {
		ctrl = conn_recvn(conn, token, tokenlen);
		if (ctrl == -1) {
			free(token);
			return -1;
		}
		ctrl = writen(fd, token, tokenlen);
		if (ctrl == -1) {
			free(token);
			return -1;
		}
		datareceived += tokenlen;
	}
	
	ctrl = conn_recvn(conn, token, last_pack);
	if (ctrl == -1) {
		free(token);
		return -1;
	}
	ctrl = writen(fd, token, last_pack);
	free(token);
	if (ctrl == -1) 
		return -1;
	
	datareceived += last_pack;
	
	if (datareceived != file_size) {
		fprintf(stderr, "Some error occurs while receiving file.\n");
		return -1;
	} else {
		printf("All file received. [%d B]\n", file_size);
		return 0;
	}
}

int conn_setTimeout(Connection conn, int timeout) {
	
	int ctrl;
	struct timeval tv;

	if (timeout < 0) {
		fprintf(stderr, "Cannot set a negative timeout\n");
		return -1;
	}
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	ctrl = setsockopt(conn.id, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (ctrl == -1) {
		report_err("Cannot set the timeout");
		return -1;
	}

	TIMEOUT = timeout;
	return 0;
}

int sendstoHost(char * string, Host host) {

	int bytes, str_len;

	struct sockaddr_in destination;

	destination.sin_family = AF_INET;
	destination.sin_port = htons(host.port);
	inet_aton(host.address, &destination.sin_addr);

	str_len = strlen(string);

	bytes = sendto(host.sock, string, str_len, 0, (SA*) &destination, (socklen_t)sizeof(destination));
	if (bytes == -1) {
		report_err("Cannot send datagram");
		return -1;
	}
	if (bytes != str_len) {
		fprintf(stderr, "Error sending datagram [%dB/%dB]\n", bytes, str_len);
		return -1;
	}

	return 0;
}

int sendntoHost(void * data, int datalen, Host host) {

	int bytes;

	struct sockaddr_in destination;

	destination.sin_family = AF_INET;
	destination.sin_port = htons(host.port);
	inet_aton(host.address, &destination.sin_addr);

	bytes = sendto(host.sock, data, datalen, 0, (SA*) &destination, (socklen_t)sizeof(destination));
	if (bytes == -1) {
		report_err("Cannot send datagram");
		return -1;
	}
	if (bytes != datalen) {
		fprintf(stderr, "Error sending datagram [%dB/%dB]\n", bytes, datalen);
		return -1;
	}

	return 0;
}

int recvsfromHost(char * string, int str_len, Host * host, int timeout) {

	int remote_len, bytes;

	struct timeval tv;
	struct sockaddr_in remote;

	if (timeout >= 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		setsockopt(host->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	}

	remote_len = sizeof(remote);

	bytes = recvfrom(host->sock, string, str_len, 0, (SA*) &remote, (socklen_t*)&remote_len);
	if (timeout && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		fprintf(stderr, "Timeout expired [%d sec]\n", timeout);
		return -1;
	}
	if (bytes == -1) {
		report_err("Cannot receive datagram");
		return -1;
	}
	
	string[bytes] = '\0';
	
	strcpy(host->address, inet_ntoa(remote.sin_addr));
	host->port = ntohs(remote.sin_port);

	return 0;
}

int recvnfromHost(void * data, int datalen, Host * host, int timeout) {
	
	int remote_len, bytes;

	struct timeval tv;
	struct sockaddr_in remote;

	if (timeout >= 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		setsockopt(host->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	}

	remote_len = sizeof(remote);

	bytes = recvfrom(host->sock, data, datalen, 0, (SA*) &remote, (socklen_t*)&remote_len);
	if (timeout && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		fprintf(stderr, "Timeout expired [%d sec]\n", timeout);
		return -1;
	}
	if (bytes == -1) {
		report_err("Cannot receive datagram");
		return -1;
	}
	
	strcpy(host->address, inet_ntoa(remote.sin_addr));
	host->port = ntohs(remote.sin_port);

	return 0;
}

/** UTILITIES **/

int checkaddress(char * address) {
	int a, b, c, d;

	if ((sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d)) == EOF) {
		fprintf(stderr, "Wrong address format\n");
		return -1;
	}

	if (CORRECT(a) && CORRECT(b) && CORRECT(c) && CORRECT(d))
		return 0;
	else {
		fprintf(stderr, "Wrong address format\n");
		return -1;
	}
}

int checkport(char * port) {
	int p = atoi(port);
	
	if (p < 0 || p > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		return -1;
	} else
		return p;
}

int readline(char * string, int str_len) {
	int nl = 0;
	
	fgets(string, str_len, stdin);
	
	nl = strcspn(string, "\n\r");
	
	if (nl != 0)
    	string[nl] = '\0';
    else {	
    	while(getchar() != '\n'){}
    	return str_len;
    }
	return nl;
}

int writen(int fd, void * buffer, int dataremaining) {
	
	int bytes;
	void * ptr = buffer;
	
	while (dataremaining > 0) {
		bytes = write(fd, ptr, dataremaining);
		if (bytes == -1) {
			report_err("Cannot write on file");
			return -1;
		}
		dataremaining -= bytes;
		ptr += bytes;
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while writing the file\n");
		return -1;
	}
	return 0;
}

int readn(int fd, void * buffer, int dataremaining) {
	
	int bytes;
	void * ptr = buffer;
	
	while (dataremaining > 0) {
		bytes = read(fd, ptr, dataremaining);
		if (bytes == -1) {
			report_err("Cannot read the file");
			return -1;
		}
		if (bytes == 0) 
			break;
		dataremaining -= bytes;
		ptr += bytes;
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while reading the file\n");
		return -1;
	}
	return 0;
}
