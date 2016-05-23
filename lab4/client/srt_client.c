//
// FILE: srt_client.c
//
// Description: this file contains client states' definition, some important data structures
// and the client SRT socket interface definitions. You need to implement all these interfaces
//
// Date: April 18, 2008
//       April 21, 2008 **Added more detailed description of prototypes fixed ambiguities** ATC
//       April 26, 2008 ** Added GBN and send buffer function descriptions **
//

#include "srt_client.h"

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
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

const char *output = "SRT Client:";

int connection;
int ind[MAX_TRANSPORT_CONNECTIONS];
client_tcb_t *tcb_table[MAX_TRANSPORT_CONNECTIONS];
pthread_mutex_t *lock[MAX_TRANSPORT_CONNECTIONS];

//sendN send from first unsent to GBN_WINDOW or TAIL
void sendN(int i) {
	while (NULL != tcb_table[i]->sendBufunSent && tcb_table[i]->unAck_segNum < GBN_WINDOW) {
		snp_sendseg(connection, &(tcb_table[i]->sendBufunSent->seg));
		printf("%s TCB[%d] Send Buffer %d!\n", output, i, tcb_table[i]->sendBufunSent->seg.header.seq_num);
		tcb_table[i]->sendBufunSent->sentTime = clock();
		tcb_table[i]->sendBufunSent = tcb_table[i]->sendBufunSent->next;
		tcb_table[i]->unAck_segNum++;
	}
}
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

void srt_client_init(int conn) {
	int i, rc;
	pthread_t thread;

	//initialize the TCB table marking all entries NULL
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++)	{
		tcb_table[i] = NULL;
		ind[i] = i;
	}
	
	//initialize the global variable
	connection = conn;

	pthread_create(&thread, NULL, seghandler, NULL);

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

			lock[i] = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(lock[i], NULL);

			client_tcb_t *tcb = (client_tcb_t *) malloc(sizeof(client_tcb_t));
			tcb->client_portNum = client_port;
			tcb->state = CLOSED;
			tcb->next_seqNum = 0;
			tcb->bufMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(tcb->bufMutex, NULL);
			tcb->sendBufHead = NULL;
			tcb->sendBufunSent = NULL;
			tcb->sendBufTail = NULL;
			tcb->unAck_segNum = 0;
			tcb_table[i] = tcb;
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
	memset(&s, 0, sizeof(seg_t));
	s.header.src_port = tcb_table[sockfd]->client_portNum;
	s.header.dest_port = tcb_table[sockfd]->svr_portNum;
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
		tm.tv_nsec = SYN_TIMEOUT;
		nanosleep(&tm, NULL);
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
	} while (p == 0);
	
	return p;
}

// Send data to a srt server. This function should use the socket ID to find the TCP entry. 
// Then It should create segBufs using the given data and append them to send buffer linked list. 
// If the send buffer was empty before insertion, a thread called sendbuf_timer 
// should be started to poll the send buffer every SENDBUF_POLLING_INTERVAL time
// to check if a timeout event should occur. If the function completes successfully, 
// it returns 1. Otherwise, it returns -1.
// 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int srt_client_send(int sockfd, void* data, unsigned int length) {
	int seg_num = (length - 1) / MAX_SEG_LEN + 1;
	int i;
	pthread_mutex_lock(tcb_table[sockfd]->bufMutex);
	for (i = 0; i < seg_num; i++) {
		segBuf_t *segbuf = (segBuf_t *) malloc(sizeof(segBuf_t));
		memset(segbuf, 0, sizeof(segBuf_t));
		segbuf->seg.header.src_port		= tcb_table[sockfd]->client_portNum;
		segbuf->seg.header.dest_port	= tcb_table[sockfd]->svr_portNum;
		segbuf->seg.header.seq_num		= tcb_table[sockfd]->next_seqNum;
		segbuf->seg.header.length		= (i == seg_num - 1 ? (length - i * MAX_SEG_LEN ): MAX_SEG_LEN);
		tcb_table[sockfd]->next_seqNum += segbuf->seg.header.length;
		segbuf->seg.header.type			= DATA;
		segbuf->seg.header.checksum		= 0;
		memcpy(segbuf->seg.data, data + i * MAX_SEG_LEN, segbuf->seg.header.length);
		segbuf->next					= NULL;
	
		if (NULL == tcb_table[sockfd]->sendBufHead) {
			tcb_table[sockfd]->sendBufHead		= segbuf;
			tcb_table[sockfd]->sendBufunSent	= segbuf;
			tcb_table[sockfd]->sendBufTail		= segbuf;
			pthread_t th;
			pthread_create(&th, NULL, sendBuf_timer, (void *) &ind[sockfd]);
		}
		else {
			tcb_table[sockfd]->sendBufTail->next	= segbuf;
			tcb_table[sockfd]->sendBufTail			= segbuf;
			if (NULL == tcb_table[sockfd]->sendBufunSent) {
				tcb_table[sockfd]->sendBufunSent	= segbuf;
			}
		}
	}
	sendN(sockfd);
	pthread_mutex_unlock(tcb_table[sockfd]->bufMutex);
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
	memset(&s, 0, sizeof(seg_t));
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
		tm.tv_nsec = FIN_TIMEOUT;
		nanosleep(&tm, NULL);
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
	} while (p == 0);

	//clear send buffer
	pthread_mutex_lock(tcb_table[sockfd]->bufMutex);
	segBuf_t *cur = tcb_table[sockfd]->sendBufHead;
	while (NULL != cur) {
		segBuf_t *temp = cur;
		cur = cur->next;
		free(temp);
	}
	pthread_mutex_unlock(tcb_table[sockfd]->bufMutex);

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
	pthread_mutex_lock(lock[sockfd]);
	if (CLOSED != tcb_table[sockfd]->state) {
		return -1;
	}
	pthread_mutex_destroy(tcb_table[sockfd]->bufMutex);
	free(tcb_table[sockfd]->bufMutex);
	free(tcb_table[sockfd]);
	tcb_table[sockfd] = NULL;
	pthread_mutex_unlock(lock[sockfd]);

	pthread_mutex_destroy(lock[sockfd]);
	free(lock[sockfd]);

	printf("%s TCB[%d] Closing Completed.\n", output, sockfd);
	return 1;
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
		int i;
		unsigned short int client_portNum	= seg_recv.header.dest_port;
		unsigned short int type				= seg_recv.header.type;
		client_tcb_t *tcb = NULL;
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			pthread_mutex_lock(lock[i]);
			if (NULL != tcb_table[i] && client_portNum == tcb_table[i]->client_portNum) {
				tcb = tcb_table[i];
				break;
			}
			pthread_mutex_unlock(lock[i]);
		}
		if (NULL == tcb) {
			printf("%s There is no TCB matching port number %d.\n", output, client_portNum);
		}
		else {
			switch (tcb->state) {
				case CLOSED:
					break;
				case SYNSENT:
					if (SYNACK == type) {
						printf("%s TCB[%d] Received SYNACK!\n", output, i);
						printf("%s TCB[%d] State (SYNSENT) ==> (CONNECTED)\n", output, i);
						tcb->state = CONNECTED;
					}
					break;
				case CONNECTED:
					if (DATAACK == type) {
						printf("%s TCB[%d] Received DATAACK! Server Expect %d.\n", output, i, seg_recv.header.ack_num);
						pthread_mutex_lock(tcb->bufMutex);
						while (NULL != tcb->sendBufHead && tcb->sendBufHead->seg.header.seq_num < seg_recv.header.ack_num) {
							printf("%s TCB[%d] Free %d.\n", output, i, tcb->sendBufHead->seg.header.seq_num);
							segBuf_t *temp = tcb->sendBufHead;
							tcb->sendBufHead = tcb->sendBufHead->next;
							free(temp);
							tcb->unAck_segNum--;
						}
						if (NULL != tcb->sendBufHead) {
							sendN(i);
						}
						pthread_mutex_unlock(tcb->bufMutex);
					}
					break;
				case FINWAIT:
					if (FINACK == type) {
						printf("%s TCB[%d] Received FINACK!\n", output, i);
						printf("%s TCB[%d] State (FINWAIT) ==> (CLOSED)\n", output, i);
						tcb->state = CLOSED;
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




// This thread continuously polls send buffer to trigger timeout events
// It should always be running when the send buffer is not empty
// If the current time -  first sent-but-unAcked segment's sent time > DATA_TIMEOUT, a timeout event occurs
// When timeout, resend all sent-but-unAcked segments
// When the send buffer is empty, this thread terminates
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void* sendBuf_timer(void* arg)
{
	int i = *(int *) arg;
	while (1) {
		struct timespec tm;
		tm.tv_sec = 0;
		tm.tv_nsec = SENDBUF_POLLING_INTERVAL;
		nanosleep(&tm, NULL);
	
		pthread_mutex_lock(tcb_table[i]->bufMutex);
		if (NULL == tcb_table[i]->sendBufHead) {
			pthread_mutex_unlock(tcb_table[i]->bufMutex);
			printf("%s TCB[%d] sendBuf_timer thread killed...\n", output, i);
			return NULL;
		}
		if (clock() - tcb_table[i]->sendBufHead->sentTime > DATA_TIMEOUT) {
			segBuf_t *cur = tcb_table[i]->sendBufHead;
			int T = tcb_table[i]->unAck_segNum;
			printf("%s TCB[%d] Resend Buffer %d...\n", output, i, cur->seg.header.seq_num);
			while (T-- > 0) {
				snp_sendseg(connection, &(cur->seg));
				cur->sentTime = clock();
				cur = cur->next;
			}
		}
		pthread_mutex_unlock(tcb_table[i]->bufMutex);
	}
	return NULL;
}
