common_obj = conn.o types.o
server_obj = server_tcp.o $(common_obj)
client_obj = client_tcp.o $(common_obj)

server : $(server_obj)
	gcc -o server $(server_obj)
	
client : $(client_obj)
	gcc -o client $(client_obj);
	
server_tcp.o client_tcp.o conn.o : conn.h

types.o : types.h

types.h : types.xdr
	rpcgen -h -o types.h types.xdr
	
types.c : types.xdr
	rpcgen -c -o types.c types.xdr

clean :
	rm server client server_tcp.o client_tcp.o conn.o
