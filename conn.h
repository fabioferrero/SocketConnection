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

typedef int Connection;

typedef struct host	{
	char * address;
	int port;
	Connection conn;
} Host;

void fatal_err(char *message);
void report_err(char *message);

Host Host_init(char * address, int port, int protocol);

Host prepareServer(int port, int protocol);

/***** TCP *****/

/* Return the established connection ID */
Connection conn_connect(char * address, int port);

/* Return the connected remote host */
Host acceptHost(Host * server);

/* Return the number of chars sended */
int conn_sends(Host host, char * string);

/* Return the number of bytes sended */
int conn_send(Host host, void * data, int datalen);

/* Return the number of characters received or
 * Retrun 0 if the connection has been closed or
 * Return -1 if an error occurs.
 * The function continue to wait data until the terminator sequence is reached
 * or the str_len is reached.
 * The string retrieved hasn't the terminator included.
 */
int conn_recvs(Host host, char * string, int str_len, char * terminator);

/* Return the number of characters received or
 * Retrun 0 if the connection has been closed or
 * Return -1 if an error occurs.
 * The function continue to wait data until str_len data are received.
 */
int conn_recvn(Host host, char * string, int str_len);

/* Close the connection to and from the host  */
int conn_close(Host host);

/***** UDP *****/

/* Return the number of bytes sended to host, or -1 if an error occurred*/
int sendstoHost(char * string, Host * host);

int sendtoHost(void * data, Host * host);

/* Return the number of bytes received from host, -1 if an error occured 
 * or 0 if the timeout expired */
int recvsfromHost(char * string, Host * host, int timeout);

int recvfromHost(void * data, Host * host, int timeout);

/* UTILITIES */

int checkaddress(char * address);


