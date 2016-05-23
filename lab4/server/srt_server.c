// FILE: srt_server.c
//
// Description: this file contains server states' definition, some important
// data structures and the server SRT socket interface definitions. You need 
// to implement all these interfaces
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 **Added GBN descriptions
//

#include "srt_server.h"

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
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// This function initializes the TCB table marking all entries NULL. It also initializes 
// a global variable for the overlay TCP socket descriptor ``conn'' used as input parameter
// for snp_sendseg and snp_recvseg. Finally, the function starts the seghandler thread to 
// handle the incoming segments. There is only one seghandler for the server side which
// handles call connections for the client.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

const char *output = "SRT Server:";

int connection;
svr_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t *lock[MAX_TRANSPORT_CONNECTIONS];

//closewait_timeout
void *timeout(void *arg) {
	int i = *(int *) arg;
	sleep(CLOSEWAIT_TIMEOUT);

	if (NULL != tcb_table[i] && CLOSEWAIT == tcb_table[i]->state) {
		printf("%s TCB[%d] CLOSEWAIT_TIMEOUT!\n", output, i);
		printf("%s TCB[%d] State (CLOSEWAIT) ==> (CLOSED)\n", output, i);
		tcb_table[i]->state = CLOSED;
	}

	return NULL;
}

void srt_server_init(int conn) {
	int i, rc;
	pthread_t thread;

	//initialize the TCB table marking all entries NULL
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)	{
		tcb_table[i]	= NULL;
	}
	
	//initialize the global variable
	connection = conn;

	pthread_create(&thread, NULL, seghandler, NULL);

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

			lock[i] = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(lock[i], NULL);

			svr_tcb_t *tcb = (svr_tcb_t *) malloc(sizeof(svr_tcb_t));
			tcb->svr_portNum = port;
			tcb->state = CLOSED;
			tcb->expect_seqNum = 0;
			tcb->recvBuf = (char *) malloc(sizeof(char) * RECEIVE_BUF_SIZE);
			tcb->usedBufLen = 0;
			tcb->bufMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(tcb->bufMutex, NULL);
			tcb_table[i] = tcb;
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
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = ACCEPT_POLLING_INTERVAL;
		nanosleep(&tm, NULL);
	}
}

// Receive data from a srt client. Recall this is a unidirectional transport
// where DATA flows from the client to the server. Signaling/control messages
// such as SYN, SYNACK, etc.flow in both directions. 
// This function keeps polling the receive buffer every RECVBUF_POLLING_INTERVAL
// until the requested data is available, then it stores the data and returns 1
// If the function fails, return -1 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int srt_server_recv(int sockfd, void* buf, unsigned int length) {
	while (1) {
		pthread_mutex_lock(lock[sockfd]);
		if (NULL == tcb_table[sockfd] || CONNECTED != tcb_table[sockfd]->state) {
			pthread_mutex_unlock(lock[sockfd]);
			return -1;
		}
		if (tcb_table[sockfd]->usedBufLen >= length) {
			break;
		}
		pthread_mutex_unlock(lock[sockfd]);
		sleep(RECVBUF_POLLING_INTERVAL);
	}
	pthread_mutex_lock(tcb_table[sockfd]->bufMutex);
	memcpy(buf, tcb_table[sockfd]->recvBuf, length);
	int i;
	for (i = length; i < tcb_table[sockfd]->usedBufLen; i++) {
		tcb_table[sockfd]->recvBuf[i - length] = tcb_table[sockfd]->recvBuf[i];
	}
	tcb_table[sockfd]->usedBufLen -= length;
	pthread_mutex_unlock(tcb_table[sockfd]->bufMutex);
	pthread_mutex_unlock(lock[sockfd]);
	return 1;
}

// Close the srt server
//
// This function calls free() to free the TCB entry. It marks that entry in TCB as NULL
// and returns 1 if succeeded (i.e., was in the right state to complete a close) and -1
// if fails (i.e., in the wrong state).
//

int srt_server_close(int sockfd) {
	pthread_mutex_lock(lock[sockfd]);
	if (CLOSED != tcb_table[sockfd]->state) {
		pthread_mutex_unlock(lock[sockfd]);
		return -1;
	}	
	pthread_mutex_destroy(tcb_table[sockfd]->bufMutex);
	free(tcb_table[sockfd]->bufMutex);
	free(tcb_table[sockfd]->recvBuf);
	free(tcb_table[sockfd]);
	tcb_table[sockfd] = NULL;
	pthread_mutex_unlock(lock[sockfd]);

	pthread_mutex_destroy(lock[sockfd]);
	free(lock[sockfd]);

	printf("%s TCB[%d] Closing Completed.\n", output, sockfd);
	return 1;
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
		int i, rc;
		unsigned short int svr_portNum	= seg_recv.header.dest_port;
		unsigned short int type			= seg_recv.header.type;
		svr_tcb_t *tcb = NULL;
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			pthread_mutex_lock(lock[i]);
			if (NULL != tcb_table[i] && svr_portNum == tcb_table[i]->svr_portNum) {
				tcb = tcb_table[i];
				break;
			}
			pthread_mutex_unlock(lock[i]);
		}
		if (NULL == tcb) {
			printf("%s There is no TCB matching port number %d.\n", output, svr_portNum);
		}
		else {
			switch (tcb->state) {
				case CLOSED:
					break;
				case LISTENING:
					if (SYN == type) {
						printf("%s TCB[%d] Received SYN!\n", output, i);
						printf("%s TCB[%d] State (LISTENING) ==> (CONNECTED)\n", output, i);
						tcb->state = CONNECTED;
						tcb->client_portNum = seg_recv.header.src_port;
						memset(&seg_send, 0, sizeof(seg_t));
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = SYNACK;
						snp_sendseg(connection, &seg_send);
					}
					break;
				case CONNECTED:
					if (SYN == type) {
						printf("%s TCB[%d] Received SYN!\n", output, i);
						memset(&seg_send, 0, sizeof(seg_t));
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = SYNACK;
						snp_sendseg(connection, &seg_send);
					}
					else if (FIN == type) {
						printf("%s TCB[%d] Received FIN!\n", output, i);
						printf("%s TCB[%d] State (CONNECTED) ==> (CLOSEWAIT)\n", output, i);
						tcb->state = CLOSEWAIT;
						pthread_t t;
						rc = pthread_create(&t, NULL, timeout, (void *) &i);
						memset(&seg_send, 0, sizeof(seg_t));
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = FINACK;
						snp_sendseg(connection, &seg_send);
					}
					else if (DATA == type) {
						pthread_mutex_lock(tcb->bufMutex);
						memset(&seg_send, 0, sizeof(seg_t));
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = DATAACK;
						printf("%s TCB[%d] Receive DATA %d ", output, i, seg_recv.header.seq_num);
						if (tcb->expect_seqNum == seg_recv.header.seq_num && tcb->usedBufLen + seg_recv.header.length <= RECEIVE_BUF_SIZE) {
							printf("As Expected. Store... ");
							memcpy(tcb->recvBuf + tcb->usedBufLen, seg_recv.data, seg_recv.header.length);
							tcb->expect_seqNum	+= seg_recv.header.length;
							tcb->usedBufLen		+= seg_recv.header.length;
						}
						else {
							printf("Not As Expected. Discard... ");
						}
						seg_send.header.ack_num = tcb->expect_seqNum;
						printf("Now Expecting %d\n", tcb->expect_seqNum);
						snp_sendseg(connection, &seg_send);
						pthread_mutex_unlock(tcb->bufMutex);
					}
					break;
				case CLOSEWAIT:
					if (FIN == type) {
						printf("%s TCB[%d] Received FIN!\n", output, i);
						memset(&seg_send, 0, sizeof(seg_t));
						seg_send.header.src_port = tcb->svr_portNum;
						seg_send.header.dest_port = tcb->client_portNum;
						seg_send.header.length = 0;
						seg_send.header.type = FINACK;
						snp_sendseg(connection, &seg_send);
					}
					break;
				default:
					break;
			}
			pthread_mutex_unlock(lock[i]);
		}
	}
	return NULL;
}

