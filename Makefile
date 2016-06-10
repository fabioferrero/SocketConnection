common_obj = conn.o

all: tcp udp

tcp : server_tcp client_tcp

udp : server_udp client_udp

server_tcp : server_tcp.o $(common_obj)
	gcc -o server_tcp server_tcp.o $(common_obj) -lpthread
	
client_tcp : client_tcp.o $(common_obj)
	gcc -o client_tcp client_tcp.o $(common_obj)
	
server_tcp.o client_tcp.o conn.o : conn.h

server_udp : server_udp.o $(common_obj)
	gcc -o server_udp server_udp.o $(common_obj)
	
client_udp : client_udp.o $(common_obj)
	gcc -o client_udp client_udp.o $(common_obj)

clean :
	rm -f server_tcp client_tcp server_tcp.o client_tcp.o \
	server_udp client_udp server_udp.o client_udp.o \
	conn.o *.txt *~
