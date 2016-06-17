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
#include <rpc/xdr.h>	// XDR

//#define TRUE 1
//#define FALSE 0

#define TCP 0
#define UDP 1

#define DATAGRAM_LEN 1024

#define NO_TIMEOUT 0

typedef struct connection {
	char address[16];
	int port;
	int id;
} Connection;

typedef struct host	{
	char address[16];
	int port;
	int sock;
} Host;

/* NOTE: all funciton return: 0 for SUCCESS and -1 for ERROR */
void fatal_err(char *message);
void report_err(char *message);

/***** SERVER *****/
/* Returns an host used to acceptConn() on TCP or recvfromHost() on UDP */
Host prepareServer(int port, int protocol); 			// EXIT on failure

/***** TCP SERVER *****/
Connection acceptConn(Host server); //TODO by reference and return value

/***** TCP CLIENT *****/
Connection conn_connect(char * address, int port);		// EXIT on failure

/***** TCP *****/
int conn_close(Connection conn);

int conn_sends(Connection conn, char * string);
int conn_sendn(Connection conn, void * data, int datalen);
int conn_sendfile(Connection conn, int fd, int file_size);
int conn_sendfile_tokenized(Connection conn, int fd, int file_size, int tokenlen);

int conn_recvs(Connection conn, char * string, int str_len, char * terminator);
int conn_recvn(Connection conn, void * data, int datalen);
int conn_recvfile(Connection conn, int fd, int file_size);
int conn_recvfile_tokenized(Connection conn, int fd, int file_size, int tokenlen);

int conn_setTimeout(Connection conn, int timeout);

/***** UDP CLIENT *****/
Host Host_init(char * address, int port);					// EXIT on failure

/***** UDP *****/
int closeHost(Host host);

int sendstoHost(char * string, Host host);
int sendntoHost(void * data, int datalen, Host host);

int recvsfromHost(char * string, int str_len, Host * host, int timeout);
int recvnfromHost(void * data, int datalen, Host * host, int timeout);

/***** XDR *****/

int xdr_sendfile(XDR * xdrOut, int fd, int file_size);
int xdr_recvfile(XDR * xdrOut, int fd, int file_size);

/** UTILITIES **/

int checkaddress(char * address);
int checkport(char * port);						// Returns the port number or -1
int readline(char * string, int str_len);		// Returns the string lenght
int writen(int fd, void * buffer, int nbyte);
int readn(int fd, void * buffer, int nbyte);

/** DNS resolve **/
char * getAddressByName(char * url);			// Returns the IP or NULL
char * nextAddress();							// Returns the IP or NULL


