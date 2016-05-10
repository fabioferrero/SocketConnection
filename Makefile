server_objects = server_tcp.o conn.o
client_objects = client_tcp.o conn.o

server : $(server_objects)
	gcc -o server $(server_objects)
	
client : $(client_objects)
	gcc -o client $(client_objects);
	
server_tcp.o client_tcp.o conn.o : conn.h

clean :
	rm server client server_tcp.o client_tcp.o conn.o
