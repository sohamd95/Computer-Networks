#include <fcntl.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <readline/readline.h>

#define SERVER_PORTAL "/tmp/chat_server"
#define ERR_FULL "ERR1"
#define MAX_MSGSIZ 128

typedef void (*sighandler_t)(int);

int id, connected, sfd;
char *name;
FILE *stream;
sighandler_t default_handler;

void connectToServer() {

	printf("\nHi %s!\nAttempting to connect to chat server...\n", name);

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(12345);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if(-1 == connect(sfd, (struct sockaddr*) &server_addr, sizeof server_addr)) {
		perror("connect(server)");
		exit(-1);
	}

	stream = fdopen(sfd, "r+");
	if(stream == NULL) {
		perror("fdopen(sfd)");
		exit(-1);
	}
	setbuf(stream, NULL);

	char response[128];

	printf("Waiting for response from server...\n");
	fgets(response, sizeof response, stream);
	if(!strcmp(response, ERR_FULL)) {
		printf("Sorry, It looks like the chatroom is full! \n");
		exit(-1);
	}
	else sscanf(response, "%i\n", &id);

	connected = 1;
	printf("\033[2J\033[1;1H\x1b[32;1mConnected!\x1b[0m\n\n");

	fprintf(stream, "%s \n", name);
}

void disconnect() {

	fprintf(stream, "exiting\n");
	fclose(stream);

	signal(SIGINT, default_handler);
	raise(SIGINT);
}

void *receiver(void *junk) {

	char *buffer = (char*) malloc(MAX_MSGSIZ*sizeof(char));

	while(connected) {
		int nbytes = recv(sfd, buffer, MAX_MSGSIZ, 0);
		buffer[nbytes] = '\n';
		buffer[nbytes+1] = '\0';

		if(nbytes == 0) {
			printf("\033[1A\033[K\nConnection to server lost!\n");
			connected = 0;
			break;
		}
		
		printf("\r\033[K%s\x1b[36;1m[%s]:\x1b[0m ", buffer, name);
	}

}

void *sender(void *junk) {

	char *buffer = (char*) malloc(MAX_MSGSIZ);
	char formattedName[128];
	sprintf(formattedName, "\x1b[36;1m[%s]:\x1b[0m ", name);

	while(connected) {
		printf("%s", formattedName);
		size_t n;
		getline(&buffer, &n, stdin);
		//buffer = readline(formattedName);

		if(!isEmpty(buffer)) {
			//send(sfd, buffer, strlen(buffer), 0);
			fprintf(stream, "%s", buffer);
			// printf("Sent!\n");
		} else printf("\033[1A\033[K");
	}
}

int isEmpty(char* string) {
	int i = 0;
	while(string[i] != '\0')
		if(isalnum(string[i++])) return 0;
	return 1;
}

int main() {

	setbuf(stdout, NULL);
	setbuf(stdin, NULL);

	name = readline("Please enter a Username: ");
	connectToServer();
	default_handler = signal(SIGINT, disconnect);

	pthread_t receiverT, senderT;
	pthread_create(&receiverT, NULL, receiver, NULL);
	pthread_create(&senderT, NULL, sender, NULL);

	pthread_join(senderT, NULL);
	pthread_join(receiverT, NULL);

	fclose(stream);

	return 0;

}
