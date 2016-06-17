common_obj = conn.o

tcp : server_tcp client_tcp

udp : server_udp client_udp

xdr : server_xdr client_xdr

all : tcp udp

server_tcp : server_tcp.o $(common_obj)
	gcc -o server_tcp server_tcp.o $(common_obj) -lpthread
	
client_tcp : client_tcp.o $(common_obj)
	gcc -o client_tcp client_tcp.o $(common_obj)
	
pre_fork : pre_fork.o $(common_obj)
	gcc -o pre_fork pre_fork.o $(common_obj)
	
server_udp : server_udp.o $(common_obj)
	gcc -o server_udp server_udp.o $(common_obj)
	
client_udp : client_udp.o $(common_obj)
	gcc -o client_udp client_udp.o $(common_obj)
	
server_xdr : server_xdr.o $(common_obj)
	gcc -o server_xdr server_xdr.o $(common_obj)

client_xdr : client_xdr.o $(common_obj)
	gcc -o client_xdr client_xdr.o $(common_obj)

server_tcp.o client_tcp.o pre_fork.o server_udp.o client_udp.o server_xdr.o client_xdr.o conn.o : conn.h

clean :
	rm -f server_tcp client_tcp server_tcp.o client_tcp.o \
	server_udp client_udp server_udp.o client_udp.o \
	server_xdr client_xdr server_xdr.o client_xdr.o \
	conn.o *.txt *~ *.avi *.pdf
