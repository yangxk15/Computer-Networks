//FILE: overlay/overlay.c
//
//Description: this file implements a ON process 
//A ON process first connects to all the neighbors and then starts listen_to_neighbor threads each of which keeps receiving the incoming packets from a neighbor and forwarding the received packets to the SNP process. Then ON process waits for the connection from SNP process. After a SNP process is connected, the ON process keeps receiving sendpkt_arg_t structures from the SNP process and sending the received packets out to the overlay network. 
//
//Date: April 28,2008

#include "overlay.h"

//you should start the ON processes on all the overlay hosts within this period of time
#define OVERLAY_START_DELAY 10

/**************************************************************/
//declare global variables
/**************************************************************/

//declare the neighbor table as global variable 
nbr_entry_t* nt; 
//declare the TCP connection to SNP process as global variable
int network_conn; 

const char *output = "Overlay:";

/**************************************************************/
//implementation overlay functions
/**************************************************************/

// This thread opens a TCP port on CONNECTION_PORT and waits for the incoming connection from all the neighbors that have a larger node ID than my nodeID,
// After all the incoming connections are established, this thread terminates 
void* waitNbrs(void* arg) {
	//put your code here
	int id = topology_getMyNodeID();
	int i;
	int nbrNum = topology_getNbrNum();
	int tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in tcpserv_addr;
	struct sockaddr_in tcpclient_addr;
	
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(CONNECTION_PORT);
	
	bind(tcpserv_sd, (struct sockaddr *) &tcpserv_addr, sizeof(tcpserv_addr));
	int count = 0;
	for (i = 0; i < nbrNum; i++) {
		if (id < nt[i].nodeID) {
			count++;
		}
	}
	listen(tcpserv_sd, 1);
	while (count --> 0) {
		socklen_t tcpclient_addr_len = sizeof(tcpclient_addr);
		int conn = accept(tcpserv_sd, (struct sockaddr *) &tcpclient_addr, &tcpclient_addr_len);
		if (conn >= 0) {
			int nodeID = topology_getNodeIDfromip(&tcpclient_addr.sin_addr);
			printf("%s Successfully accepted a connection from node %d.\n", output, nodeID);
			nt_addconn(nt, nodeID, conn);
		}
		else {
			printf("%s Acceptance from node %d failed.\n", output, nt[i].nodeID);
			return NULL;
		}
	}
	close(tcpserv_sd);
	return NULL;
}

// This function connects to all the neighbors that have a smaller node ID than my nodeID
// After all the outgoing connections are established, return 1, otherwise return -1
int connectNbrs() {
	//put your code here
	int id = topology_getMyNodeID();
	int i;
	int nbrNum = topology_getNbrNum();
	for (i = 0; i < nbrNum; i++) {
		if (id > nt[i].nodeID) {
			int conn = socket(AF_INET, SOCK_STREAM, 0);
			struct sockaddr_in servaddr;
			struct hostent *hostInfo;
			hostInfo = gethostbyname(inet_ntoa(*(struct in_addr *) &nt[i].nodeIP));
			servaddr.sin_family = hostInfo->h_addrtype;
			memcpy((char *) &servaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
			servaddr.sin_port = htons(CONNECTION_PORT);
			if (connect(conn, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
				printf("%s Connection to node %d failed.\n", output, nt[i].nodeID);
				return -1;
			}
			else {
				nt_addconn(nt, nt[i].nodeID, conn);
				printf("%s Successfully connected to node %d.\n", output, nt[i].nodeID);
			}
		}
	}
	return 1;
}

//Each listen_to_neighbor thread keeps receiving packets from a neighbor. It handles the received packets by forwarding the packets to the SNP process.
//all listen_to_neighbor threads are started after all the TCP connections to the neighbors are established 
void* listen_to_neighbor(void* arg) {
	//put your code here
	int i = *(int *) arg;
	snp_pkt_t pkt;
	while (1 == recvpkt(&pkt, nt[i].conn)) {
		printf("%s Received a pkt from node %d!", output, nt[i].nodeID);
		if (network_conn == -1) {
			printf("\tNo local SNP!\n");
		}
		else {
			printf("\tForward it to local SNP!\n");
			forwardpktToSNP(&pkt, network_conn);
		}
	}
	printf("%s node %d is lost! No longer listen to it.\n", output, nt[i].nodeID);
	nt[i].conn = -1;
	return NULL;
}

//This function opens a TCP port on OVERLAY_PORT, and waits for the incoming connection from local SNP process. After the local SNP process is connected, this function keeps getting sendpkt_arg_ts from SNP process, and sends the packets to the next hop in the overlay network. If the next hop's nodeID is BROADCAST_NODEID, the packet should be sent to all the neighboring nodes.
void waitNetwork() {
	//put your code here
	int tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in tcpserv_addr;
	
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(OVERLAY_PORT);
	
	bind(tcpserv_sd, (struct sockaddr *) &tcpserv_addr, sizeof(tcpserv_addr));
	listen(tcpserv_sd, 1);
	while (1) {
		struct sockaddr_in tcpclient_addr;
		socklen_t tcpclient_addr_len = sizeof(tcpclient_addr);
		network_conn = accept(tcpserv_sd, (struct sockaddr *) &tcpclient_addr, &tcpclient_addr_len);
		if (network_conn < 0) {
			return;
		}
		printf("%s Local connection established!\n", output);
		snp_pkt_t pkt;
		int nextNode;
		while (1 == getpktToSend(&pkt, &nextNode, network_conn)) {
			printf("%s Received a pkt from local SNP!", output);
			if (nextNode == BROADCAST_NODEID) {
				printf("\tBroadcasting...\n");
				int i;
				int nbrNum = topology_getNbrNum();
				for (i = 0; i < nbrNum; i++) {
					if (nt[i].conn != -1)
						sendpkt(&pkt, nt[i].conn);
				}
			}
			else {
				int i;
				int nbrNum = topology_getNbrNum();
				for (i = 0; i < nbrNum; i++) {
					if (nt[i].nodeID == nextNode) {
						if (nt[i].conn != -1) {
							printf("\tSend to node %d...", nt[i].nodeID);
							sendpkt(&pkt, nt[i].conn);
						}
					}
				}
				printf("\n");
			}
		}
		printf("%s Local SNP is lost!\n", output);
		close(network_conn);
		network_conn = -1;
	}
}

//this function stops the overlay
//it closes all the connections and frees all the dynamically allocated memory
//it is called when receiving a signal SIGINT
void overlay_stop() {
	//put your code here
	if (network_conn != -1) {
		close(network_conn);
		network_conn = -1;
	}
	nt_destroy(nt);
	printf("%s Stop.\n", output);
}

int main() {
	//start overlay initialization
	printf("%s Node %d initializing...\n", output, topology_getMyNodeID());	

	//create a neighbor table
	nt = nt_create();
	//initialize network_conn to -1, means no SNP process is connected yet
	network_conn = -1;
	
	//register a signal handler which is sued to terminate the process
	signal(SIGINT, overlay_stop);

	//print out all the neighbors
	int nbrNum = topology_getNbrNum();
	int i;
	int *idx = (int *) malloc(sizeof(int) * nbrNum);
	for(i = 0; i < nbrNum; i++) {
		idx[i] = i;
		printf("%s neighbor %d: %d, %s, %d\n", output, i + 1, nt[i].nodeID, inet_ntoa(*(struct in_addr *) &nt[i].nodeIP), nt[i].conn);
	}

	//start the waitNbrs thread to wait for incoming connections from neighbors with larger node IDs
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void *) 0);

	//wait for other nodes to start
	sleep(OVERLAY_START_DELAY);
	
	//connect to neighbors with smaller node IDs
	connectNbrs();

	//wait for waitNbrs thread to return
	pthread_join(waitNbrs_thread, NULL);	

	//at this point, all connections to the neighbors are created
	
	//create threads listening to all the neighbors
	for(i = 0; i < nbrNum; i++) {
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void *) (&idx[i]));
		printf("%s neighbor %d: %d, %s, %d\n", output, i + 1, nt[i].nodeID, inet_ntoa(*(struct in_addr *) &nt[i].nodeIP), nt[i].conn);
	}
	printf("%s node initialized...\n", output);
	printf("%s waiting for connection from SNP process...\n", output);

	//waiting for connection from  SNP process
	waitNetwork();
}
