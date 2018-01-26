#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define MAX_CLIENTS 8
#define MAX_MSGSIZ 128
#define ERR_FULL "ERR1"

typedef void (*sighandler_t)(int);

struct client {
	
	int id;
	int sfd;
	char name[64];
	FILE* stream;

};

struct client *clients[MAX_CLIENTS];
int numClients, maxFd, sfd;
pthread_mutex_t mutex;
sighandler_t old_handler;

void* connector(void* junk) {

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd == -1) {
		perror("socket()");
		exit(-1);
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(12345);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if(-1 == bind(sfd, (struct sockaddr*) &server_addr, sizeof server_addr)) {
		perror("bind()");
		exit(-1);
	}
	listen(sfd, 4);

	int i;
	printf("Server Started...\n");

	while(1) {

		//Block till client arrives (accept())
		printf("\nWaiting for client...\n");
		
		struct sockaddr_in client_addr;
		socklen_t nbytes = sizeof client_addr;
		int nsfd = accept(sfd, (struct sockaddr *) &client_addr, &nbytes);
		if(-1 == nsfd) {
			perror("accept(sfd)");
			exit(-1);
		}

		//Open Communication stream
		FILE * stream = fdopen(nsfd, "r+");
		if(stream == NULL) {
			perror("fdopen(nsfd)");
			exit(-1);
		}
		setbuf(stream, NULL);

		printf("Client Arrived. Stream opened\n");

		pthread_mutex_lock(&mutex);

		int id = 0;
		while(clients[id] != NULL && id < MAX_CLIENTS) id++;

		if(id == MAX_CLIENTS) {
			printf("No more clients can join\n");
			fprintf(stream, "%s\n", ERR_FULL);
			fclose(stream);
			continue;
		}

		fprintf(stream, "%i\n", id);

		clients[id] = (struct client*) malloc(sizeof(struct client));
		clients[id]->id = id;
		clients[id]->sfd = nsfd;
		clients[id]->stream = stream;

		char msgBuf[64];
		fgets (msgBuf, 64 , clients[id]->stream);
		sscanf(msgBuf, "%s \n", clients[id]->name);

		if(nsfd > maxFd) maxFd = nsfd;
		numClients++;

		pthread_mutex_unlock(&mutex);

		int j;
		for(j = 0 ; j<MAX_CLIENTS ; j++)
			if(clients[j] != NULL && j!=id)
				fprintf(clients[j]->stream, "\x1b[32;1m%s\x1b[37;1m has joined.\x1b[0m\n", clients[id]->name);

		printf("\tName: %s\n\tID: %i\n\n", clients[id]->name, id);
		printf("\x1b[32;1m%s\x1b[37;1m has joined.\x1b[0m\n", clients[id]->name);
	}
}

void disconnect(struct client **cliPtr) {

	char name[64];
	struct client *cli = *(cliPtr);
	strcpy(name, cli->name);
	int cli_sfd = cli->sfd;

	//Close Socket
	//shutdown(fileno(cli->stream), 2);
	fclose(cli->stream);

	//Free memory
	free(cli);
	*(cliPtr) = NULL;

	//Find new maxFd
	if(maxFd == cli_sfd) {
		maxFd = -2;
		int i;
		for(i=0 ; i<MAX_CLIENTS ; i++) {
			if(clients[i] != NULL && clients[i]->sfd > maxFd)
				maxFd = clients[i]->sfd;
		}
	}

	//Construct leave message
	char leaveMsg[128];
	sprintf(leaveMsg, "\x1b[32;1m%s\x1b[37;1m has left.\x1b[0m\n", name);
	printf("%s", leaveMsg);

	//Inform all others that a person has left
	int i;
	for(i=0 ; i<MAX_CLIENTS ; i++)
		if(clients[i] != NULL)
			fprintf(clients[i]->stream, "%s", leaveMsg);

	numClients--;
}

void* msgHandlerFunc(void *junk) {

	int i;
	char *buffer = (char*) malloc(MAX_MSGSIZ*sizeof(char));
	fd_set fds;
	struct timeval tv;

	while(1) {

		if(numClients == 0) continue;

		//Lock clients array to prevent connectionMngr thread from modifying it
		pthread_mutex_lock(&mutex);
		//Wait for a client to send something

		//Reset fdset
		FD_ZERO(&fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0.1*1000*1000;
		for(i=0 ; i<MAX_CLIENTS ; i++) {
			if(clients[i] != NULL) {
				FD_SET(clients[i]->sfd ,&fds);
			}
		}

		int n = select(maxFd+1, &fds, NULL, NULL, &tv);

		if(n == 0) {
			pthread_mutex_unlock(&mutex);
			continue;
		}

		if(n == -1) {
			perror("select(fds)");
			exit(-1);
		}

		for(i=0 ; i<MAX_CLIENTS ; i++) {
			if(clients[i] != NULL && FD_ISSET(clients[i]->sfd, &fds)) {

				int nbytes = recv(clients[i]->sfd, buffer, MAX_MSGSIZ, 0);
				buffer[nbytes] = '\0';
				printf("Received from %i: %s", i, buffer);

				if(!strcmp(buffer, "exiting\n")) {
					disconnect(&clients[i]);
				}
				else {
					char formattedMsg[MAX_MSGSIZ];
					sprintf(formattedMsg, "\x1b[37;1m[%s]:\x1b[0m %s", clients[i]->name, buffer);

					int j;
					for(j = 0 ; j<MAX_CLIENTS ; j++)
						if(j != i && clients[j] != NULL){
							send(clients[j]->sfd, formattedMsg, MAX_MSGSIZ, 0);
						}
				}
			}
		}
		//Unlock fd array
		pthread_mutex_unlock(&mutex);
	}

	printf("MsgHandler EXITING!\n");
}

void closeConnections() {

	shutdown(sfd, 2);
	close(sfd);

	int i;
	for(i=0 ; i<MAX_MSGSIZ ; i++)
		if(clients[i] != NULL) {
			shutdown(clients[i]->sfd, 2);
			close(clients[i]->sfd);
		}

	signal(SIGINT, old_handler);
	raise(SIGINT);
}

int main(int argc, char *argv[]) {

	char *sAddr;
	int sPort;
	if(argc == 1) {
		sAddr = "127.0.0.1";
		sPort = 12345;
	}
	setbuf(stdout, NULL);
	printf("\033[2J\033[1;1H");

	numClients = 0;
	maxFd = -1;
	int i;
	for(i=0 ; i<MAX_CLIENTS ; i++)
		clients[i] = NULL;

	old_handler = signal(SIGINT, closeConnections);
	pthread_t connectionMngr, msgHandler;

	pthread_mutex_init(&mutex, NULL);
	pthread_create(&connectionMngr, NULL, connector, NULL);
	pthread_create(&msgHandler, NULL, msgHandlerFunc, NULL);

	pthread_join(connectionMngr, NULL);
	pthread_join(msgHandler, NULL);

	return 0;
}
