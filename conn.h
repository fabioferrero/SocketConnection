/*	conn.h 
	written by Fabio Ferrero
*/
#include <stdio.h> 		
#include <stdlib.h>		// exit();

#include <unistd.h>		// close();
#include <time.h>
#include <errno.h>		// errno
#include <string.h>
#include <fcntl.h>		// open();
#include <stdint.h>		// uint32_t

#include <sys/stat.h>	// files info
#include <sys/types.h>  // pthread_x_t

#include <sys/wait.h>	// wait();
#include <signal.h>		// signal();

#include <netinet/in.h> // Address conversion

#define TRUE 1
#define FALSE 0

#define TCP 0
#define UDP 1

#define DATAGRAM_LEN 1024
#define TOKEN 65536

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

/* NOTE: all funciton return: 0 for SUCCESS and -1 for ERROR */
void fatal_err(char *message);
void report_err(char *message);

/***** SERVER *****/

Host prepareServer(int port, int protocol);
int closeServer(Host server);
Connection acceptConn(Host server);

/***** CLIENT *****/

Host Host_init(char * address, int port, int protocol);
Connection conn_connect(char * address, int port);
int conn_close(Connection conn);

/***** TCP *****/

int conn_sends(Connection conn, char * string);
int conn_sendn(Connection conn, void * data, int datalen);
int conn_sendfile(Connection conn, int fd, int file_size);
int conn_sendfile_tokenized(Connection conn, int fd, int file_size, int tokenlen);

int conn_recvs(Connection conn, char * string, int str_len, char * terminator);
int conn_recvn(Connection conn, void * data, int datalen);
int conn_recvfile(Connection conn, int fd, int file_size);
int conn_recvfile_tokenized(Connection conn, int fd, int file_size, int tokenlen);

int conn_setTimeout(Connection conn, int timeout);

/***** UDP *****/

int sendstoHost(char * string, Host * host);
int sendntoHost(void * data, int datalen, Host * host);

int recvsfromHost(char * string, Host * host, int timeout);
int recvnfromHost(void * data, int datalen, Host * host, int timeout);

/** UTILITIES **/

int checkaddress(char * address);
int checkport(char * port);
int readline(char * string, int str_len);
int writen(int fd, void * buffer, int nbyte);
int readn(int fd, void * buffer, int nbyte);

/* TODO DNS resolve */

