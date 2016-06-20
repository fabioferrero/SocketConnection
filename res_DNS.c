#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

int main(int argc, char *argv[]) {

	char *ip = malloc(46*sizeof(char));
	
	getAddressByName(argv[1], ip);
	if (ip == NULL) 
		return 0;
		
	printf("IP: %s\n", ip);
	
	while((nextAddress(ip)) != NULL) {
		printf("IP: %s\n", ip);
	}
	
	free(ip);
	return 0;
}
