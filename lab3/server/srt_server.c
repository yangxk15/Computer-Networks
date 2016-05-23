#include "srt_server.h"


/*interfaces to application layer*/

//
//
//  SRT socket API for the server side application.
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
//  GOAL: Your job is to design and implement the prototypes below - fill in the code.
//

// srt server initialization
//
// This function initializes the TCB table marking all entries NULL. It also initializes
// a global variable for the overlay TCP socket descriptor ‘‘conn’’ used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to
// handle the incoming segments. There is only one seghandler for the server side which
// handles call connections for the client.
//

const char *output = "SRT Server:";

int connection;
svr_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t lock =  PTHREAD_MUTEX_INITIALIZER;

//closewait_timeout
void *timeout(void *arg) {
	int i = *(int *) arg;
	sleep(CLOSEWAIT_TIME);

	if (0 != pthread_mutex_lock(&lock)) {
		printf("%s TCB[%d] CLOSEWAIT_TIMEOUT: Acquire lock failed!\n", output, i);
		return NULL;
	}

	//critical section
	if (NULL != tcb_table[i] && CLOSEWAIT == tcb_table[i]->state) {
		printf("%s TCB[%d] CLOSEWAIT_TIMEOUT!\n", output, i);
		printf("%s TCB[%d] State (CLOSEWAIT) ==> (CLOSED)\n", output, i);
		tcb_table[i]->state = CLOSED;
	}

	if (0 != pthread_mutex_unlock(&lock)) {
		printf("%s TCB[%d] CLOSEWAIT_TIMEOUT: Release lock failed!\n", output, i);
		return NULL;
	}
	return NULL;
}

void srt_server_init(int conn) {
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

// Create a server sock
//
// This function looks up the client TCB table to find the first NULL entry, and creates
// a new TCB entry using malloc() for that entry. All fields in the TCB are initialized
// e.g., TCB state is set to CLOSED and the server port set to the function call parameter
// server port.  The TCB table entry index should be returned as the new socket ID to the server
// and be used to identify the connection on the server side. If no entry in the TCB table
// is available the function returns -1.

int srt_server_sock(unsigned int port) {
	int i;
	int index = -1;

	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if (tcb_table[i] == NULL) {
			index = i;
			tcb_table[i] = (svr_tcb_t *) malloc(sizeof(svr_tcb_t));
			memset(tcb_table[i], 0, sizeof(svr_tcb_t));
			tcb_table[i]->svr_portNum = port;
			tcb_table[i]->state = CLOSED;
			break;
		}
	}

	printf("%s TCB[%d] of Port (%d) Created.\n", output, index, port);
	return index;
}

// Accept connection from srt client
//
// This function gets the TCB pointer using the sockfd and changes the state of the connetion to
// LISTENING. It then starts a timer to ‘‘busy wait’’ until the TCB’s state changes to CONNECTED
// (seghandler does this when a SYN is received). It waits in an infinite loop for the state
// transition before proceeding and to return 1 when the state change happens, dropping out of
// the busy wait loop. You can implement this blocking wait in different ways, if you wish.
//

int srt_server_accept(int sockfd) {
	svr_tcb_t *tcb = tcb_table[sockfd];
	printf("%s TCB[%d] State (CLOSED) ==> (LISTENING)\n", output, sockfd);
	tcb->state = LISTENING;
	while (1) {
		if (CONNECTED == tcb->state) {
			printf("%s TCB[%d] Accept Connection!\n", output, sockfd);
			return 1;
		}
		sleep(1);
	}
}

// Receive data from a srt client
//
// Receive data to a srt client. Recall this is a unidirectional transport
// where DATA flows from the client to the server. Signaling/control messages
// such as SYN, SYNACK, etc.flow in both directions. You do not need to implement
// this for Lab3. We will use this in Lab4 when we implement a Go-Back-N sliding window.
//
int srt_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// Close the srt server
//
// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//

int srt_server_close(int sockfd) {
	unsigned int state = tcb_table[sockfd]->state;

	free(tcb_table[sockfd]);
	tcb_table[sockfd] = NULL;

	printf("%s TCB[%d] Closing Completed.\n", output, sockfd);
	return CLOSED == state ? 1 : -1;
}

// Thread handles incoming segments
//
// This is a thread  started by srt_server_init(). It handles all the incoming
// segments from the client. The design of seghanlder is an infinite loop that calls snp_recvseg(). If
// snp_recvseg() fails then the overlay connection is closed and the thread is terminated. Depending
// on the state of the connection when a segment is received  (based on the incoming segment) various
// actions are taken. See the client FSM for more details.
//

void *seghandler(void* arg) {
	seg_t seg_recv;
	seg_t seg_send;
	while (1 == snp_recvseg(connection, &seg_recv)) {
		if (0 != pthread_mutex_lock(&lock)) {
			printf("%s SegHandler: Acquire lock failed!\n", output);
			return NULL;
		}
		int i, rc;
		unsigned short int svr_portNum	= seg_recv.header.dest_port;
		unsigned short int type			= seg_recv.header.type;
		svr_tcb_t *tcb = NULL;
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if (NULL != tcb_table[i] && svr_portNum == tcb_table[i]->svr_portNum) {
				tcb = tcb_table[i];
				break;
			}
		}
		if (NULL == tcb) {
			printf("%s There is no TCB matching port number %d.\n", output, svr_portNum);
		}
		else {
			switch (type) {
				case SYN:
					if (LISTENING == tcb->state || CONNECTED == tcb->state) {
						printf("%s TCB[%d] Received SYN!\n", output, i);
						if (LISTENING == tcb->state) {
							printf("%s TCB[%d] State (LISTENING) ==> (CONNECTED)\n", output, i);
							tcb->state = CONNECTED;
							tcb->client_portNum = seg_recv.header.src_port;
						}
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = SYNACK;
						if (-1 == snp_sendseg(connection, &seg_send)) {
							printf("%s TCB[%d] Sending SYNACK Failure!\n", output, i);
						}
						else {
							printf("%s TCB[%d] Sending SYNACK Success!\n", output, i);
						}
					}
					break;
				case FIN:
					if (CONNECTED == tcb->state || CLOSEWAIT == tcb->state) {
						printf("%s TCB[%d] Received FIN!\n", output, i);
						if (CONNECTED == tcb->state) {
							printf("%s TCB[%d] State (CONNECTED) ==> (CLOSEWAIT)\n", output, i);
							tcb->state = CLOSEWAIT;
							pthread_t t;
							rc = pthread_create(&t, NULL, timeout, (void *) &i);
							if (rc) {
								printf("%s TCB[%d] SegHandler: Create new thread failed!\n", output, i);
								exit(-1);
							}
						}
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = FINACK;
						if (-1 == snp_sendseg(connection, &seg_send)) {
							printf("%s TCB[%d] Sending FINACK Failure!\n", output, i);
						}
						else {
							printf("%s TCB[%d] Sending FINACK Success!\n", output, i);
						}
					}
					break;
				default:
					break;
			}
		}
		if (0 != pthread_mutex_unlock(&lock)) {
			printf("Server_TCB_Table: Release lock failed!\n");
			return NULL;
		}
	}
	return NULL;
}

