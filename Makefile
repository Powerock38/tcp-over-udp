all: client server

client: client.o
	gcc -o client client.o

server: server.o
	gcc -o server server.o

client.o: client.c shared.h
	gcc -c -Wall client.c

server.o: server.c shared.h
	gcc -c -Wall server.c

clean:
	rm -f *.o client server