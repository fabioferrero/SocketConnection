#include <stdio.h> 		
#include <stdlib.h>		// exit()
#include <unistd.h>		// close()
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>		//open()
#include <stdint.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define TCP 0
#define UDP 1

typedef struct connection {
	char address[16];
	int port;
	int id;
} Connection;

typedef struct host	{
	char address[16];
	int port;
	int conn;
} Host;

void fatal_err(char *message);
void report_err(char *message);

/***** SERVER *****/

Host prepareServer(int port, int protocol);
Host acceptHost(Host * server);

/***** CLIENT *****/

Host Host_init(char * address, int port, int protocol);

Connection conn_connect(char * address, int port);

int conn_close(Host host);

/***** TCP *****/

int conn_sends(Host host, char * string);
int conn_send(Host host, void * data, int datalen);

int conn_recvs(Host host, char * string, int str_len, char * terminator);
int conn_recvn(Host host, char * string, int str_len);

/***** UDP *****/

int sendstoHost(char * string, Host * host);
int sendtoHost(void * data, Host * host);

int recvsfromHost(char * string, Host * host, int timeout);
int recvfromHost(void * data, Host * host, int timeout);

/** UTILITIES **/

int checkaddress(char * address);
int checkport(char * port);
int readline(char * string, int str_len);


