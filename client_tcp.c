#include <stdio.h> 		
#include <stdlib.h>
#include <string.h>

#include "conn.h"

int main(int argc, char *argv[]) {

	Connection conn;
	
	if (argc != 3) {
		fprintf(stderr, "syntax: %s <address> <port>\n", argv[0]); 
		exit(-1);
	}
	
	checkaddress(argv[1]);
	
	conn = conn_connect(argv[1], checkport(argv[2]));
	
	return 0;
}
