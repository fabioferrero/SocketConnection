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
#define QUEUE_SIZE 10
#define CORRECT(x) (x >= 0 && x < 256)

void fatal_err(char *message) {
	fprintf(stderr, "%s - (Error %d)\n", message, errno);
	perror("");
	exit(1);
}

void report_err(char *message) {
	fprintf(stderr, "%s - (Error %d)\n", message, errno);
	perror("");
	return;
}

Connection conn_connect(char * address, int port) {
	
	Connection conn;
	int retval;
	struct sockaddr_in destination;

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

Connection acceptConn(Host * server) {

	Connection conn;

	struct sockaddr_in addr;
	int addr_len = sizeof(addr);

	conn.id = accept(server->conn, (SA*)&addr, (socklen_t*)&addr_len);
	if (conn.id == -1) {
		report_err("Cannot accept a connection");
	} else {
		strcpy(conn.address, inet_ntoa(addr.sin_addr));
		conn.port = ntohs(addr.sin_port);
		printf("New connection from %s:%d [fd=%d]\n", conn.address, conn.port, conn.id);
	}

	return conn;
}

int conn_sends(Connection conn, char * string) {

	int datasended, dataremaining, counter = 0;
	char *ptr = string;

	dataremaining = strlen(string);

	while (dataremaining != 0) {
		datasended = send(conn.id, ptr, dataremaining, 0);
		if (datasended <= 0) { //error
			report_err("Cannot send string");
			break;
		} else {
			counter += datasended;
			dataremaining -= datasended;
			ptr += datasended;
		}
	}

	return counter;
}

int conn_sendn(Connection conn, void * data, int dataremaining) {

	int datasended, counter = 0;
	void *ptr = data;

	while (dataremaining != 0) {
		datasended = send(conn.id, ptr, dataremaining, 0);
		if (datasended <= 0) { //error
			report_err("Cannot send data");
			break;
		} else {
			counter += datasended;
			dataremaining -= datasended;
			ptr += datasended;
		}
	}

	return counter;
}

/* Return the number of characters received or
 * Retrun 0 if the connection has been closed or
 * Return -1 if an error occurs.
 * The function continue to wait data until the terminator sequence is reached
 * or the str_len is reached.
 * The string retrieved hasn't the terminator included.
 */
/* TODO change the number of character used in order to read char-by-char
 * TODO add the possibility to have the terminator = NULL
 * TODO add the timeout
 */
int conn_recvs(Connection conn, char * string, int str_len, char * terminator) {

	int i, datareceived, ter_len, counter = 0, equal = 1;
	char * ptr = string;

	ter_len = strlen(terminator);

	for (i=0; i< str_len; i++)
		string[i]='0';

	while (str_len > 0) {
		datareceived = recv(conn.id, ptr, str_len, 0);
		if (datareceived == 0) {
			printf("Connection closed by party\n");
			string[counter] = '\0';
			return 0;
		}
		if (datareceived == -1) {
			report_err("Cannot receive data");
			string[counter] = '\0';
			return -1;
		}
		counter += datareceived;
		if (string[counter-ter_len] == terminator[0]) {
			// Check if the terminator is reached
			for(i=0;i<ter_len;i++) {
				if (string[counter-ter_len+i] != terminator[i])
					equal = 0;
			}
			if (equal)
				break;
		}
		ptr += datareceived;
		str_len -= datareceived;
	}

	string[counter-ter_len] = '\0';

	return counter;
}

/* Return the number of characters received or
 * Retrun 0 if the connection has been closed or
 * Return -1 if an error occurs.
 * The function continue to wait data until str_len data are received.
 */
/*
 * TODO add the timeout
 */
int conn_recvn(Connection conn, void * data, int dataremaining) {

	int datareceived, counter = 0;
	void * ptr = data;

	while (dataremaining > 0) {
		datareceived = recv(conn.id, ptr, dataremaining, 0);
		if (datareceived == 0) {
			printf("Connection closed by party\n");
			return 0;
		}
		if (datareceived == -1) {
			report_err("Cannot receive data");
			return -1;
		}
		counter += datareceived;
		ptr += datareceived;
		dataremaining -= datareceived;
	}

	return counter;
}

int conn_close(Connection conn) {
	if (close(conn.id)) {
		report_err("Cannot close the connection");
		return -1;
	}
	return 0;
}

int sendstoHost(char * string, Host * host) {

	int bytes, str_len;

	struct sockaddr_in destination;

	if (host->conn == UDP) { //The connection is not created yet
		host->conn = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (host->conn == -1) {
			report_err("Cannot set the connection");
			return -1;
		} else {
			printf("Connection set [fd=%d]\n", host->conn);
		}
	}

	destination.sin_family = AF_INET;
	destination.sin_port = htons(host->port);
	inet_aton(host->address, &destination.sin_addr);

	str_len = strlen(string);

	bytes = sendto(host->conn, string, str_len, 0, (SA*) &destination, (socklen_t)sizeof(destination));

	if (bytes == -1) {
		report_err("Cannot send datagram");
		return -1;
	} else if (bytes != str_len) {
		fprintf(stderr, "Error sending datagram [%dB/%dB]\n", bytes, str_len);
		return 0;
	} else {
		printf("Datagram sended [%dB/%dB]\n", bytes, str_len);
	}

	return bytes;
}

int sendtoHost(void * data, Host * host) {
	return 0;
}

int recvsfromHost(char * string, Host * host, int timeout) {

	int remote_len, bytes;

	struct timeval tv;
	struct sockaddr_in remote;

	if (host->conn == UDP) { //The connection is not created yet
		host->conn = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (host->conn == -1) {
			report_err("Cannot set the connection");
			return -1;
		} else {
			printf("Connection set [fd=%d]\n", host->conn);
		}
	}

	if (timeout != 0)
		tv.tv_sec = timeout;
	else
		tv.tv_sec = 0;
	tv.tv_usec = 0;
	setsockopt(host->conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	remote_len = sizeof(remote);

	bytes = recvfrom(host->conn, string, DATAGRAM_LEN, 0, (SA*) &remote, (socklen_t*)&remote_len);

	if (timeout && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		fprintf(stderr, "Timeout expired [%d sec]\n", timeout);
		return 0;
	} else if (bytes != -1) {
		string[bytes] = '\0';
		strcpy(host->address, inet_ntoa(remote.sin_addr));
		host->port = ntohs(remote.sin_port);
		printf("Datagram received [%dB]\n", bytes);
	} else {
		report_err("Cannot receive datagram");
	}

	return bytes;
}

/* TODO implementation */
int recvfromHost(void * data, Host * host, int timeout) {
	return 0;
}

Host Host_init(char * address, int port, int protocol) {

	Host h;
	char prot[4];

	strcpy(h.address, address);
	h.port = port;

	switch (protocol) {
		case TCP:
			h.conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			strcpy(prot, "TCP");
			break;
		case UDP:
			h.conn = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			strcpy(prot, "UDP");
			break;
		default:
			fprintf(stderr, "Invalid protocol\n");
			h.conn = -1;
	}
	if (h.conn == -1)
		fatal_err("Cannot create the connection");
	else {
		printf("Connection set [fd=%d:%s]\n", h.conn, prot);
	}

	return h;
}

Host prepareServer(int port, int protocol) {

	Host host;
	char prot[4];

	struct sockaddr_in addr;

	if (port < 1 || port > 65536) {
		fprintf(stderr, "Invalid port number: valid range [1 - 65536]\n");
		exit(-1);
	}

	host.port = port;

	switch (protocol) {
		case TCP:
			host.conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			strcpy(prot, "TCP");
			break;

		case UDP:
			host.conn = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			strcpy(prot, "UDP");
			break;

		default:
			fprintf(stderr, "Invalid protocol\n");
			exit(-1);
	}
	if (host.conn == -1)
		fatal_err("Cannot set the connection");
	else
		printf("Connection set [fd=%d:%s]\n", host.conn, prot);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(host.port);
	addr.sin_addr.s_addr = INADDR_ANY;

	strcpy(host.address, inet_ntoa(addr.sin_addr));

	if (bind(host.conn, (SA*)&addr, sizeof(addr)))
		fatal_err("Cannot prepare the server");
	printf("Server prepared [address %s:%d]\n", host.address, host.port);

	if (protocol == TCP)
		if (listen(host.conn, QUEUE_SIZE))
			fatal_err("Cannot listen on specified address");

	return host;
}

int checkaddress(char * address) {
	int a, b, c, d;

	if ((sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d)) == EOF) {
		fprintf(stderr, "Wrong address format\n");
		exit(-1);
	}

	if (CORRECT(a) && CORRECT(b) && CORRECT(c) && CORRECT(d))
		return 0;
	else {
		fprintf(stderr, "Wrong address format\n");
		exit(-1);
	}
}

int checkport(char * port) {
	int p = atoi(port);
	
	if (p < 0 || p > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	} else
		return p;
}

int readline(char * string, int str_len) {
	char * nl;
	
	fgets(string, str_len, stdin);
	
	nl = strchr(string, '\n');
	
	if (nl != 0)
    	*nl = '\0';
    else while(getchar() != '\n'){}
    
	return nl-string; //lenght until \n
}

int writen(int fd, void * buffer, int dataremaining) {
	
	int bytes, datawritten = 0;
	void * ptr = buffer;
	
	while (dataremaining > 0) {
		bytes = write(fd, ptr, dataremaining);
		if (bytes == -1) {
			report_err("Cannot write on file");
			return -1;
		}
		datawritten += bytes;
		dataremaining -= bytes;
		ptr += bytes;
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while writing the file\n");
		return datawritten;
	}
	return datawritten;
}

int readn(int fd, void * buffer, int dataremaining) {
	
	int bytes, dataread = 0;
	void * ptr = buffer;
	
	while (dataremaining > 0) {
		bytes = read(fd, ptr, dataremaining);
		if (bytes == -1) {
			report_err("Cannot read the file");
			return -1;
		} else if (bytes == 0) break;
		else {
			dataread += bytes;
			dataremaining -= bytes;
			ptr += bytes;
		}
	}
	if (dataremaining != 0) {
		fprintf(stderr, "Some error occurs while reading the file\n");
		return dataread;
	}
	return dataread;
}
