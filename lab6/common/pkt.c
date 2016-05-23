// File pkt.c
// May 03, 2010

#include "pkt.h"

//send function
int sendfunc(void *ptr, int length, int connection)
{
	char bufstart[2];
	char bufend[2];
	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	
	static pthread_mutex_t* lock = NULL;
	if (lock == NULL)
	{
		lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(lock, NULL);
	}
	pthread_mutex_lock(lock);
	if (send(connection, bufstart, 2, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	if(send(connection, ptr, length, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	if(send(connection, bufend, 2, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	pthread_mutex_unlock(lock);
		
	return 1;
}

//recv function
int recvfunc(void *ptr, int length, int connection)
{
	char buf[length + 2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connection,&c,1,0)>0) {
		if (state == 0) {
		        if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				memcpy(ptr, buf, idx - 1);
				state = 0;
				idx = 0;
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}

// overlay_sendpkt() is called by the SNP process to request the ON 
// process to send a packet out to the overlay network. The 
// ON process and SNP process are connected with a local TCP connection. 
// In overlay_sendpkt(), the packet and its next hop's nodeID are encapsulated 
// in a sendpkt_arg_t data structure and sent over this TCP connection to the ON process. 
// The parameter overlay_conn is the TCP connection's socket descriptior 
// between the SNP process and the ON process.
// When sending the sendpkt_arg_t data structure over the TCP connection between the SNP 
// process and the ON process, use '!&'  and '!#' as delimiters. 
// Send !& sendpkt_arg_t structure !# over the TCP connection.
// Return 1 if sendpkt_arg_t data structure is sent successfully, otherwise return -1.
int overlay_sendpkt(int nextNodeID, snp_pkt_t* pkt, int overlay_conn)
{
	sendpkt_arg_t pkt_a;
	pkt_a.nextNodeID = nextNodeID;
	memcpy(&pkt_a.pkt, pkt, sizeof(snp_pkt_t));
	return sendfunc((void *) &pkt_a, sizeof(pkt_a.nextNodeID) + sizeof(pkt->header) + pkt->header.length, overlay_conn);
}


// overlay_recvpkt() function is called by the SNP process to receive a packet 
// from the ON process. The parameter overlay_conn is the TCP connection's socket 
// descriptior between the SNP process and the ON process. The packet is sent over 
// the TCP connection between the SNP process and the ON process, and delimiters 
// !& and !# are used. 
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point 
// PKTSTART2 -- '!' received, expecting '&' to receive data 
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data 
// Return 1 if a packet is received successfully, otherwise return -1.
int overlay_recvpkt(snp_pkt_t* pkt, int overlay_conn)
{
	return recvfunc((void *) pkt, sizeof(snp_pkt_t), overlay_conn);
}

// This function is called by the ON process to receive a sendpkt_arg_t data structure.
// A packet and the next hop's nodeID is encapsulated  in the sendpkt_arg_t structure.
// The parameter network_conn is the TCP connection's socket descriptior between the
// SNP process and the ON process. The sendpkt_arg_t structure is sent over the TCP 
// connection between the SNP process and the ON process, and delimiters !& and !# are used. 
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point 
// PKTSTART2 -- '!' received, expecting '&' to receive data 
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data
// Return 1 if a sendpkt_arg_t structure is received successfully, otherwise return -1.
int getpktToSend(snp_pkt_t* pkt, int* nextNode, int network_conn)
{
	sendpkt_arg_t pkt_a;
	if (-1 == recvfunc((void *) &pkt_a, sizeof(sendpkt_arg_t), network_conn))
		return -1;
	*nextNode = pkt_a.nextNodeID;
	memcpy(pkt, &pkt_a.pkt, sizeof(snp_pkt_t));
	return 1;
}




// forwardpktToSNP() function is called after the ON process receives a packet from 
// a neighbor in the overlay network. The ON process calls this function 
// to forward the packet to SNP process. 
// The parameter network_conn is the TCP connection's socket descriptior between the SNP 
// process and ON process. The packet is sent over the TCP connection between the SNP process 
// and ON process, and delimiters !& and !# are used. 
// Send !& packet data !# over the TCP connection.
// Return 1 if the packet is sent successfully, otherwise return -1.
int forwardpktToSNP(snp_pkt_t* pkt, int network_conn)
{
	return sendfunc((void *) pkt, sizeof(pkt->header) + pkt->header.length, network_conn);
}



// sendpkt() function is called by the ON process to send a packet 
// received from the SNP process to the next hop.
// Parameter conn is the TCP connection's socket descritpor to the next hop node.
// The packet is sent over the TCP connection between the ON process and a neighboring node,
// and delimiters !& and !# are used. 
// Send !& packet data !# over the TCP connection
// Return 1 if the packet is sent successfully, otherwise return -1.
int sendpkt(snp_pkt_t* pkt, int conn)
{
	return sendfunc((void *) pkt, sizeof(pkt->header) + pkt->header.length, conn);
}



// recvpkt() function is called by the ON process to receive 
// a packet from a neighbor in the overlay network.
// Parameter conn is the TCP connection's socket descritpor to a neighbor.
// The packet is sent over the TCP connection  between the ON process and the neighbor,
// and delimiters !& and !# are used. 
// To receive the packet, this function uses a simple finite state machine(FSM)
// PKTSTART1 -- starting point 
// PKTSTART2 -- '!' received, expecting '&' to receive data 
// PKTRECV -- '&' received, start receiving data
// PKTSTOP1 -- '!' received, expecting '#' to finish receiving data 
// Return 1 if the packet is received successfully, otherwise return -1.
int recvpkt(snp_pkt_t* pkt, int conn)
{
	return recvfunc((void *) pkt, sizeof(snp_pkt_t), conn);
}

