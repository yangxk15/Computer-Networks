#include "srt_client.h"


/*interfaces to application layer*/

//
//
//  SRT socket API for the client side application.
//  ===================================
//
//  In what follows, we provide the prototype definition for each call and limited pseudo code representation
//  of the function. This is not meant to be comprehensive - more a guideline.
//
//  You are free to design the code as you wish.
//
//  NOTE: When designing all functions you should consider all possible states of the FSM using
//  a switch statement (see the Lab3 assignment for an example). Typically, the FSM has to be
// in a certain state determined by the design of the FSM to carry out a certain action.
//
//  GOAL: Your goal for this assignment is to design and implement the 
//  prototypes below - fill the code.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// srt client initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the client side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

const char *output = "SRT Client:";

int connection;
client_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void srt_client_init(int conn) {
	int i, rc;
	pthread_t thread;

	//initialize the TCB table marking all entries NULL
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)	{
		tcb_table[i] = NULL;
	}
	
	//initialize the global variable
	connection = conn;

	rc = pthread_create(&thread, NULL, seghandler, NULL);
	if (rc) {
		printf("%s Create new thread failed!\n", output);
		exit(-1);
	}

	printf("%s Initialization Completed.\n", output);
	return;
}

// Create a client tcb, return the sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the client port set to the function call parameter
// client port.  The TCB table entry index should be returned as the new socket ID to the client
// and be used to identify the connection on the client side. If no entry in the TC table
// is available the function returns -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_sock(unsigned int client_port) {
	int i;
	int index = -1;

	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if (tcb_table[i] == NULL) {
			index = i;
			tcb_table[i] = (client_tcb_t *) malloc(sizeof(client_tcb_t));
			memset(tcb_table[i], 0, sizeof(client_tcb_t));
			tcb_table[i]->client_portNum = client_port;
			tcb_table[i]->state = CLOSED;
			break;
		}
	}

	printf("%s TCB[%d] of Port (%d) Created.\n", output, index, client_port);
	return index;
}

// Connect to a srt server
//
// This function is used to connect to the server. It takes the socket ID and the
// server’s port number as input parameters. The socket ID is used to find the TCB entry.
// This function sets up the TCB’s server port number and a SYN segment to send to
// the server using snp_sendseg(). After the SYN segment is sent, a timer is started.
// If no SYNACK is received after SYNSEG_TIMEOUT timeout, then the SYN is
// retransmitted. If SYNACK is received, return 1. Otherwise, if the number of SYNs
// sent > SYN_MAX_RETRY,  transition to CLOSED state and return -1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//


int srt_client_connect(int sockfd, unsigned int server_port) {
	tcb_table[sockfd]->svr_portNum = server_port;
	seg_t s;
	s.header.src_port = tcb_table[sockfd]->client_portNum;
	s.header.dest_port = server_port;
	s.header.length = 0;
	s.header.type = SYN;
	if (-1 == snp_sendseg(connection, &s)) {
		printf("%s TCB[%d] First Try: Sending SYN Failure!\n", output, sockfd);
	}
	else {
		printf("%s TCB[%d] First Try: Sending SYN Success!\n", output, sockfd);
	}
	printf("%s TCB[%d] State (CLOSED) ==> (SYNSENT)\n", output, sockfd);
	tcb_table[sockfd]->state = SYNSENT;
	int retry = 0;
	int p = 0;
	do {
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = SYNSEG_TIMEOUT_NS;
		nanosleep(&tm, NULL);
		if (0 != pthread_mutex_lock(&lock)) {
			printf("%s TCB[%d] Connecting Retry: Acquire lock failed!\n", output, sockfd);
			return -1;
		}
		if (CONNECTED == tcb_table[sockfd]->state) {
			p = 1;
		}
		else if (++retry > SYN_MAX_RETRY) {
			printf("%s TCB[%d] Exceeded Retry Limit!\n", output, sockfd);
			tcb_table[sockfd]->state = CLOSED;
			p = -1;
		}
		else  {
			if (-1 == snp_sendseg(connection, &s)) {
				printf("%s TCB[%d] Retry [%d]: Sending SYN Failure!\n", output, sockfd, retry);
			}
			else {
				printf("%s TCB[%d] Retry [%d]: Sending SYN Success!\n", output, sockfd, retry);
			}
		}
		if (0 != pthread_mutex_unlock(&lock)) {
			printf("%s TCB[%d] Connecting Retry: Release lock failed!\n", output, sockfd);
			return -1;
		}
	} while (p == 0);
	
	return p;
}

// Send data to a srt server
//
// Send data to a srt server. You do not need to implement this for Lab3.
// We will use this in Lab4 when we implement a Go-Back-N sliding window.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// Disconnect from a srt server
//
// This function is used to disconnect from the server. It takes the socket ID as
// an input parameter. The socket ID is used to find the TCB entry in the TCB table.
// This function sends a FIN segment to the server. After the FIN segment is sent
// the state should transition to FINWAIT and a timer started. If the
// state == CLOSED after the timeout the FINACK was successfully received. Else,
// if after a number of retries FIN_MAX_RETRY the state is still FINWAIT then
// the state transitions to CLOSED and -1 is returned.

int srt_client_disconnect(int sockfd) {
	seg_t s;
	s.header.src_port = tcb_table[sockfd]->client_portNum;
	s.header.dest_port = tcb_table[sockfd]->svr_portNum;
	s.header.length = 0;
	s.header.type = FIN;
	if (-1 == snp_sendseg(connection, &s)) {
		printf("%s TCB[%d] First Try: Sending FIN Failure!\n", output, sockfd);
	}
	else {
		printf("%s TCB[%d] First Try: Sending FIN Success!\n", output, sockfd);
	}
	printf("%s TCB[%d] State (CONNECTED) ==> (FINWAIT)\n", output, sockfd);
	tcb_table[sockfd]->state = FINWAIT;
	int retry = 0;
	int p = 0;
	do {
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = FINSEG_TIMEOUT_NS;
		nanosleep(&tm, NULL);
		if (0 != pthread_mutex_lock(&lock)) {
			printf("%s TCB[%d] Disconnecting Retry: Acquire lock failed!\n", output, sockfd);
			return -1;
		}
		if (CLOSED == tcb_table[sockfd]->state) {
			p = 1;
		}
		else if (++retry > FIN_MAX_RETRY) {
			printf("%s TCB[%d] Exceeded Retry Limit!\n", output, sockfd);
			tcb_table[sockfd]->state = CLOSED;
			p = -1;
		}
		else  {
			if (-1 == snp_sendseg(connection, &s)) {
				printf("%s TCB[%d] Retry [%d]: Sending FIN Failure!\n", output, sockfd, retry);
			}
			else {
				printf("%s TCB[%d] Retry [%d]: Sending FIN Success!\n", output, sockfd, retry);
			}
		}
		if (0 != pthread_mutex_unlock(&lock)) {
			printf("%s TCB[%d] Disconnecting Retry: Release lock failed!\n", output, sockfd);
			return -1;
		}
	} while (p == 0);
	
	return p;
}


// Close srt client

// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_close(int sockfd) {
	unsigned int state = tcb_table[sockfd]->state;

	free(tcb_table[sockfd]);
	tcb_table[sockfd] = NULL;

	printf("%s TCB[%d] Closing Completed.\n", output, sockfd);
	return CLOSED == state ? 1 : -1;
}

// The thread handles incoming segments
//
// This is a thread  started by srt_client_init(). It handles all the incoming
// segments from the server. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void* arg) {
	seg_t seg_recv;
	while (1 == snp_recvseg(connection, &seg_recv)) {
		if (0 != pthread_mutex_lock(&lock)) {
			printf("%s SegHandler: Acquire lock failed!\n", output);
			return NULL;
		}
		int i;
		unsigned short int client_portNum	= seg_recv.header.dest_port;
		unsigned short int type				= seg_recv.header.type;
		client_tcb_t *tcb = NULL;
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if (NULL != tcb_table[i] && client_portNum == tcb_table[i]->client_portNum) {
				tcb = tcb_table[i];
				break;
			}
		}
		if (NULL == tcb) {
			printf("%s There is no TCB matching port number %d.\n", output, client_portNum);
		}
		else {
			switch (type) {
				case SYNACK:
					if (SYNSENT == tcb->state) {
						printf("%s TCB[%d] Received SYNACK!\n", output, i);
						printf("%s TCB[%d] State (SYNSENT) ==> (CONNECTED)\n", output, i);
						tcb->state = CONNECTED;
					}
					break;
				case FINACK:
					if (FINWAIT == tcb->state) {
						printf("%s TCB[%d] Received FINACK!\n", output, i);
						printf("%s TCB[%d] State (FINWAIT) ==> (CLOSED)\n", output, i);
						tcb->state = CLOSED;
					}
					break;
				default:
					break;
			}
		}
		if (0 != pthread_mutex_unlock(&lock)) {
			printf("%s SegHandler: Release lock failed!\n", output);
			return NULL;
		}
	}
	return NULL;
}



