/**  The reverse-engineered client  **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* Define server ip and port */
#define SERVER_IP "129.170.213.101"
#define SERVER_PORT 47789

#define ERROR -1

/* sensorList for output result, while sendingList for sending request to server */
const char *sensorList[]  = {"WATER TEMPERATURE", "REACTOR TEMPERATURE", "POWER LEVEL"};
const char *sendingList[] = {"WATER TEMPERATURE\n", "REACTOR TEMPERATURE\n", "POWER LEVEL\n"};

int get_connect(struct sockaddr_in *s) {
	/* Create the client socket */
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		printf("CREATING SOCKET FAILED!\n");
		printf("ERRNO: %d\n", errno);
		exit(ERROR);
	}

	/* Connect to the server socket with the client socket file descriptor */
	if (connect(socket_fd, (struct sockaddr *) s, sizeof(*s)) < 0) {
		printf("CONNECTING SERVER FAILED!\n");
		printf("ERRNO: %d\n", errno);
		exit(ERROR);
	}
	return socket_fd;
}



/* execute_request takes the user's selection as input and start to cummunicate with server through socket */
void execute_request(int selection) {

	int i;
	int l;

	const char *auth_sp = "AUTH secretpassword\n";
	const char *auth_n	= "AUTH networks\n";
	const char *close_r	= "CLOSE\n";

	char buffer[BUFSIZ];	//buffer used for receiving bytes from the server

	struct sockaddr_in server;

	/* Initialize the server socket */
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(SERVER_IP);

	int socket_fd = get_connect(&server);

	/* Send 'secret password' to the server */
	send(socket_fd, auth_sp, strlen(auth_sp), 0);

	/* Receive the bytes from the server and extract the new port number */
	int new_port = 0;
	memset(buffer, 0, BUFSIZ);
	l = recv(socket_fd, buffer, BUFSIZ, 0);
	for (i = 0; i < l; i++) {
		if (buffer[i] >= '0' && buffer[i] <= '9') {
			new_port = new_port * 10 + (buffer[i] - '0');
		}
	}

	/* Update the port number of the server socket */
	server.sin_port = htons(new_port);

	/* Close the old client socket and shut down the old connection */
	close(socket_fd);
	shutdown(socket_fd, 0);

	socket_fd = get_connect(&server);

	/* Send 'networks' to the server and receive 'success' from the server */
	send(socket_fd, auth_n, strlen(auth_n), 0);
	recv(socket_fd, buffer, BUFSIZ, 0);

	/* Send the query request to the server */
	send(socket_fd, sendingList[selection - 1], strlen(sendingList[selection - 1]), 0);

	/* Receive the result from the server */
	memset(buffer, 0, BUFSIZ);
	recv(socket_fd, buffer, BUFSIZ, 0);
	char *result = strchr(&buffer[0], ' ');

	/* Record the current time */
	time_t current_time = time(NULL);
	char *time = ctime(&current_time);
	time[strlen(time) - 1] = '\0';

	/* Output the time and the result */
	printf("\tThe last %s was taken %s and was %s\n", sensorList[selection - 1], time, result + 1);

	/* Send 'close' to the server */
	send(socket_fd, close_r, strlen(close_r),  0);
	
	/* Destroy the client socket and shut down the connection */
	close(socket_fd);
	shutdown(socket_fd, 0);
}

int main() {

	printf(
			"WELCOME TO THE THREE MILE ISLAND SENSOR NETWORK\n"
	       	"\n"
	       	"\n"
	);

	do {

		int selection = 0;

		printf(
				"Which sensor would you like to read:\n"
				"\n"
				"\t(1) Water temperature\n"
				"\t(2) Reactor temperature\n"
				"\t(3) Power level\n"
				"\n"
				"Selection: "
		);

		/* if input string is not a readable integer, abort the current string */
		if (scanf("%d", &selection) != 1) {
			char c;
			do {
				c = getchar();	//keep reading to erase the illegal input
			} while(c != '\n' && c != ' ' && c != EOF);
		}

		printf("\n");

		/* Deal with the user request */
		if (selection == 1 || selection == 2 || selection == 3) {
			execute_request(selection);
		}
		else {
			printf("\n*** Invalid selection.\n");
		}

		printf("\n");

	} while (1);

	return 0;
}
