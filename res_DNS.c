#include "conn.h"

#define MAX_FILENAME 100
#define TOKEN 65536

int main(int argc, char *argv[]) {

	char *ip;
	
	ip = getAddressByName(argv[1]);
	if (ip == NULL) 
		return 0;
		
	printf("IPv4: %s\n", ip);
	while((ip = nextAddress()) != NULL) {
		printf("IP: %s\n", ip);
	}
	
	ip = getAddressByName6(argv[1]);
	if (ip == NULL) 
		return 0;
		
	printf("IPv6: %s\n", ip);
	while((ip = nextAddress()) != NULL) {
		printf("IP: %s\n", ip);
	}
	
	free(ip);
	return 0;
}
