/*	conn.c 
	written by Fabio Ferrero
*/
#include "conn.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h> // Address conversion

#include <netdb.h>		// DNS

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
 	
 	// Returns number of ready fds, and sets contains only ready fds, 0 if timeout
 	int select(int nfd, fd_set * readfd, fd_set * writefd, fd_set * exceptfd, tv * timeout);
*/

#define SA struct sockaddr
#define QUEUE_SIZE 32

#define CORRECT(x) (x >= 0 && x < 256)
#define TIMEOUT_EXP (TIMEOUT && (errno == EAGAIN || errno == EWOULDBLOCK))

int isIPv4(char * address);

uint8_t forceIPv4 = TRUE;

uint32_t TIMEOUT = 0;

struct addrinfo * host_info = NULL;
struct hostent * h_info = NULL;

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
	int ctrl, n = 1;
	
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	
	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}

	switch (protocol) {
		case TCP:
			if (forceIPv4)
				host.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			else
				host.sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			strcpy(prot, "TCP");
			break;

		case UDP:
			if (forceIPv4)
				host.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			else
				host.sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			strcpy(prot, "UDP");
			ctrl = setsockopt(host.sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &n, sizeof(n));
			if (ctrl == -1)
				report_err("Cannot reuse address");
			break;

		default:
			fprintf(stderr, "Invalid protocol\n");
			exit(-1);
	}
	if (host.sock == -1)
		fatal_err("Cannot set the server socket");
	else if (protocol == TCP)
		printf("[Connection set on %s]\n", prot);

	if (forceIPv4) {
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(port);
		addr4.sin_addr.s_addr = INADDR_ANY;
	} else {
		bzero(&addr6, sizeof(addr6));
		addr6.sin6_flowinfo = 0;
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port);
		addr6.sin6_addr = in6addr_any;
	}
	
	if (protocol == TCP) {
		/* On UDP the host structure is used to store the remote host address,
		   so don't fill that field */
		host.port = port;
		if (forceIPv4)
			inet_ntop(AF_INET, &(addr4.sin_addr), host.address, 42);
		else
			inet_ntop(AF_INET6, &(addr6.sin6_addr), host.address, 42);
	}
	if (forceIPv4) 
		ctrl = bind(host.sock, (SA*)&addr4, sizeof(addr4));
	else
		ctrl = bind(host.sock, (SA*)&addr6, sizeof(addr6));
	if (ctrl != 0)
		fatal_err("Cannot prepare the server");

	if (protocol == TCP) {
		printf("[Server prepared on port %d]\n", host.port);
		if (listen(host.sock, QUEUE_SIZE))
			fatal_err("Cannot listen on specified address");
	} else 
		printf("[Enabled UDP on port %d]\n", port);
	
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

	struct sockaddr_storage addr;
	int addr_len = sizeof(addr);

	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;

	conn.sock = accept(server.sock, (SA*)&addr, (socklen_t*)&addr_len);
	if (conn.sock == -1) {
		report_err("Cannot accept a connection");
	} else {
		if (forceIPv4) {
			addr4 = (struct sockaddr_in*) &addr;
			strcpy(conn.address, inet_ntoa(addr4->sin_addr));
			conn.port = ntohs(addr4->sin_port);
		} else {
			addr6 = (struct sockaddr_in6*) &addr;
			inet_ntop(AF_INET6, &(addr6->sin6_addr), conn.address, 46);
			conn.port = ntohs(addr6->sin6_port);
		}
		printf("[New connection from %s:%d]\n", conn.address, conn.port);
	}

	return conn;
}

Host Host_init(char * address, int port) {

	Host h;

	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}
	
	strcpy(h.address, address);
	h.port = port;
	
	if (isIPv4(address)) {
		h.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	} else {
		h.sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	}
	if (h.sock == -1)
		fatal_err("Cannot set the host socket");

	return h;
}

Connection conn_connect(char * address, int port) {
	
	Connection conn;
	int retval;
	struct sockaddr_in dest4;
	struct sockaddr_in6 dest6;
	
	if (port < 0 || port > 65536) {
		fprintf(stderr, "Wrong port specification or format\n");
		exit(-1);
	}
	
	if (isIPv4(address)) {
		dest4.sin_family = AF_INET;
		dest4.sin_port = htons(port);
		inet_aton(address, &dest4.sin_addr);

		conn.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (conn.sock == -1)
			fatal_err("Cannot create the connection");
			
		retval = connect(conn.sock, (SA*)&dest4, sizeof(dest4));
	} else {
		dest6.sin6_family = AF_INET6;
		dest6.sin6_port = htons(port);
		inet_pton(AF_INET6, address, &dest6.sin6_addr);

		conn.sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (conn.sock == -1)
			fatal_err("Cannot create the connection");
			
		retval = connect(conn.sock, (SA*)&dest6, sizeof(dest6));
	}

	if (retval == -1)
		fatal_err("Cannot connect to endpoint");
	
	strcpy(conn.address, address);
	conn.port = port;
	printf("[Connected to %s:%d]\n", conn.address, conn.port);
	
	return conn;
}

int conn_close(Connection conn) {
	if (close(conn.sock)) {
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
		datasended = send(conn.sock, ptr, dataremaining, 0);
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
		datasended = send(conn.sock, ptr, dataremaining, 0);
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
		ctrl = conn_sendn(conn, token, bytes);
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
		datareceived = recv(conn.sock, ptr, sizeof(char), 0);
		if (TIMEOUT_EXP) {
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
		datareceived = recv(conn.sock, ptr, dataremaining, 0);
		if (TIMEOUT_EXP) {
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
	ctrl = setsockopt(conn.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (ctrl == -1) {
		report_err("Cannot set the timeout");
		return -1;
	}

	TIMEOUT = timeout;
	return 0;
}

int sendstoHost(char * string, Host host) {

	int bytes, str_len;

	struct sockaddr_in dest4;
	struct sockaddr_in6 dest6;
	
	str_len = strlen(string);
	
	if (isIPv4(host.address)) {
		dest4.sin_family = AF_INET;
		dest4.sin_port = htons(host.port);
		inet_aton(host.address, &dest4.sin_addr);

		bytes = sendto(host.sock, string, str_len, 0, (SA*) &dest4, (socklen_t)sizeof(dest4));
	} else {
		dest6.sin6_family = AF_INET6;
		dest6.sin6_port = htons(host.port);
		inet_pton(AF_INET6, host.address, &dest6.sin6_addr);

		bytes = sendto(host.sock, string, str_len, 0, (SA*) &dest6, (socklen_t)sizeof(dest6));
	}
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

	struct sockaddr_in dest4;
	struct sockaddr_in6 dest6;
	
	if (isIPv4(host.address)) {
		dest4.sin_family = AF_INET;
		dest4.sin_port = htons(host.port);
		inet_aton(host.address, &dest4.sin_addr);

		bytes = sendto(host.sock, data, datalen, 0, (SA*) &dest4, (socklen_t)sizeof(dest4));
	} else {
		dest6.sin6_family = AF_INET6;
		dest6.sin6_port = htons(host.port);
		inet_pton(AF_INET6, host.address, &dest6.sin6_addr);

		bytes = sendto(host.sock, data, datalen, 0, (SA*) &dest6, (socklen_t)sizeof(dest6));
	}
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
	struct sockaddr_storage remote;
	
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;

	if (timeout >= 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		bytes = setsockopt(host->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (bytes == -1)
			report_err("Cannot set the timeout");
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

	if (forceIPv4) {
		addr4 = (struct sockaddr_in*) &remote;
		strcpy(host->address, inet_ntoa(addr4->sin_addr));
		host->port = ntohs(addr4->sin_port);
	} else {
		addr6 = (struct sockaddr_in6*) &remote;
		inet_ntop(AF_INET6, &(addr6->sin6_addr), host->address, 46);
		host->port = ntohs(addr6->sin6_port);
	}

	return 0;
}

int recvnfromHost(void * data, int datalen, Host * host, int timeout) {
	
	int remote_len, bytes;

	struct timeval tv;
	struct sockaddr_storage remote;
	
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;

	if (timeout >= 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		bytes = setsockopt(host->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (bytes == -1)
			report_err("Cannot set the timeout");
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
	
	if (forceIPv4) {
		addr4 = (struct sockaddr_in*) &remote;
		strcpy(host->address, inet_ntoa(addr4->sin_addr));
		host->port = ntohs(addr4->sin_port);
	} else {
		addr6 = (struct sockaddr_in6*) &remote;
		inet_ntop(AF_INET6, &(addr6->sin6_addr), host->address, 46);
		host->port = ntohs(addr6->sin6_port);
	}

	return 0;
}

/***** XDR *****/

int xdr_sendfile(XDR * xdrOut, int fd, int file_size) {
	
	unsigned int tokenlen = 4096;
	int sended_bytes = 0, count = 0;
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
		if (!xdr_bytes(xdrOut, &token, (u_int*) &bytes, ~0)) {
			free(token);
			report_err("Cannot send on XDR");
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

int xdr_recvfile(XDR * xdrIn, int fd, int file_size) {
	
	unsigned int last_pack, tokenlen = 4096;
	int ctrl, datareceived = 0;
	char * token;

	token = malloc(tokenlen*sizeof(char));
	if (token == NULL) {
		report_err("Cannot allocate memory for token");
		return -1;
	}
	
	last_pack = file_size % tokenlen;
	
	while (datareceived != file_size - last_pack) {
		if (!xdr_bytes(xdrIn, &token, (u_int*) &tokenlen, ~0)) {
			free(token);
			report_err("Cannot receive on XDR");
			return -1;
		}
		ctrl = writen(fd, token, tokenlen);
		if (ctrl == -1) {
			free(token);
			return -1;
		}
		datareceived += tokenlen;
	}
	
	if (!xdr_bytes(xdrIn, &token, (u_int*) &last_pack, ~0)) {
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

int isIPv4(char * address) {
	int a, b, c, d;

	if ((sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d)) == EOF) {
		return FALSE;
	}

	if (CORRECT(a) && CORRECT(b) && CORRECT(c) && CORRECT(d))
		return TRUE;
	else {
		return FALSE;
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

char * getAddressByName4(char * url) {
	
	struct in_addr *ip, **ips;
	
	h_info = gethostbyname(url);
	if (h_info == NULL) {
		fprintf(stderr, "Cannot resolve the name %s\n", url);
		return NULL;
	}
	/* Use the length as a counter for display the correct address */
	h_info->h_length = 1;
	
	ips = (struct in_addr**)h_info->h_addr_list;
	ip = ips[0];
	
	if (ip == NULL)
		return NULL;
		
	return inet_ntoa(*ip);
}

char * nextAddress4() {

	struct in_addr *ip, **ips;
	
	if (h_info == NULL) {
		fprintf(stderr, "Address not previously specified.\n");
		return NULL;
	}
	
	ips = (struct in_addr**)h_info->h_addr_list;
	ip = ips[h_info->h_length++];
	
	if (ip == NULL)
		return NULL;

	return inet_ntoa(*ip);
}

char * getAddressByName(char * url, char * ip_addr) {

	struct addrinfo hints, info, *h6_info = NULL, *tmp;
	int ctrl;
	
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;
	
	if (ip_addr == NULL) {
		report_err("No memory allocated for IP address");
		return NULL;
	} 
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	ctrl = getaddrinfo(url, NULL, &hints, &host_info);
	if (ctrl != 0) {
		//fprintf(stderr, "Cannot resolve the IPv4 address %s\n%s\n", url, gai_strerror(ctrl));
		host_info = NULL;
	}
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	
	ctrl = getaddrinfo(url, NULL, &hints, &h6_info);
	if (ctrl != 0) {
		//fprintf(stderr, "Cannot resolve the IPv6 address %s\n%s\n", url, gai_strerror(ctrl));
		h6_info = NULL;
	}
	
	if (host_info != NULL) {
		for (tmp = host_info; tmp->ai_next != NULL; tmp = tmp->ai_next) {} 
			// Find the last element of the list;

		tmp->ai_next = h6_info;	
	} else if (h6_info != NULL) {
		host_info = h6_info;
	} else { 
		return NULL;
	}
	
	info = *host_info;
	
	switch (info.ai_family) {
		case AF_INET:
			ipv4 = (struct sockaddr_in *) info.ai_addr;
			if (inet_ntop(AF_INET, &(ipv4->sin_addr), ip_addr, 46) == NULL) {
				report_err("Cannot convert IPv4 address");
				ip_addr = NULL;
			}
			break;
		case AF_INET6:
			ipv6 = (struct sockaddr_in6 *) info.ai_addr;
			if (inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_addr, 46) == NULL) {
				report_err("Cannot convert IPv6 address");
				ip_addr = NULL;
			}
			break;
		default:
			fprintf(stderr, "Address family unrecognized.\n"); 
			ip_addr = NULL;
			break;
	}
	
	free(host_info);
	host_info = info.ai_next;
	
	return ip_addr;
}

char * nextAddress(char * ip_addr) {

	struct addrinfo info;
	
	struct sockaddr_in *ipv4;
	struct sockaddr_in6 *ipv6;
	
	if (ip_addr == NULL) {
		report_err("No memory allocated for IP address");
		return NULL;
	}
	
	if (host_info == NULL) {
		// Last element of the list
		freeaddrinfo(host_info);
		return NULL;
	}
	
	info = *host_info;
	
	switch (info.ai_family) {
		case AF_INET:
			ipv4 = (struct sockaddr_in *) info.ai_addr;
			if (inet_ntop(AF_INET, &(ipv4->sin_addr), ip_addr, 46) == NULL) {
				report_err("Cannot convert IPv4 address");
				ip_addr = NULL;
			}
			break;
		case AF_INET6:
			ipv6 = (struct sockaddr_in6 *) info.ai_addr;
			if (inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_addr, 46) == NULL) {
				report_err("Cannot convert IPv6 address");
				ip_addr = NULL;
			}
			break;
		default:
			fprintf(stderr, "Address family unrecognized.\n"); 
			ip_addr = NULL;
			break;
	}
	
	free(host_info);
	host_info = info.ai_next;
	
	return ip_addr;
}

void useIPv6(int use6) {
	if (use6)
		forceIPv4 = FALSE;
	else 
		forceIPv4 = TRUE;
}

