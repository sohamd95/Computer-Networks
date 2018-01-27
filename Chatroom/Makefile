CC=gcc

default: server client

client: client.c
	$(CC) client.c -o client.out -lpthread -lreadline

server: server.c
	$(CC) server.c -o server.out -lpthread

clean:
	rm -f *.out
