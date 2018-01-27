CC=gcc

default: server client

client: client.c
	$(CC) client.c -o client -lpthread -lreadline

server: server.c
	$(CC) server.c -o server -lpthread

clean:
	rm -f server client
