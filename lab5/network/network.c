//FILE: network/network.c
//
//Description: this file implements SNP process  
//
//Date: April 29,2008



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "network.h"

/**************************************************************/
//declare global variables
/**************************************************************/
int overlay_conn; 		//connection to the overlay

const char *output = "Network:";


/**************************************************************/
//implementation network layer functions
/**************************************************************/


//this function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT
//connection descriptor is returned if success, otherwise return -1
int connectToOverlay() { 
	//put your code here
	int conn = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(OVERLAY_PORT);
	if (connect(conn, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		printf("%s Connection to the overlay failed.\n", output);
		return -1;
	}
	printf("%s Successfully connected to the overlay.\n", output);
	return conn;
}

//This thread sends out route update packets every ROUTEUPDATE_INTERVAL time
//In this lab this thread only broadcasts empty route update packets to all the neighbors, broadcasting is done by set the dest_nodeID in packet header as BROADCAST_NODEID
void* routeupdate_daemon(void* arg) {
	//put your code here
	while (1) {
		snp_pkt_t pkt;
		pkt.header.src_nodeID	= topology_getMyNodeID();
		pkt.header.dest_nodeID	= BROADCAST_NODEID;
		pkt.header.length		= sizeof(pkt_routeupdate_t);
		pkt.header.type			= ROUTE_UPDATE;
		memset(pkt.data, 0, pkt.header.length);
		if (1 == overlay_sendpkt(BROADCAST_NODEID, &pkt, overlay_conn)) {
			printf("%s Broadcast a route update packet!\n", output);
		}
		else {
			printf("%s Route update failed!\n", output);
			return NULL;
		}
		sleep(ROUTEUPDATE_INTERVAL);
	}
	return NULL;
}

//this thread handles incoming packets from the ON process
//It receives packets from the ON process by calling overlay_recvpkt()
//In this lab, after receiving a packet, this thread just outputs the packet received information without handling the packet 
void* pkthandler(void* arg) {
	snp_pkt_t pkt;

	while(overlay_recvpkt(&pkt,overlay_conn)>0) {
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
	}
	printf("%s Overlay is lost! Exit.\n", output);
	close(overlay_conn);
	overlay_conn = -1;
	pthread_exit(NULL);
}

//this function stops the SNP process 
//it closes all the connections and frees all the dynamically allocated memory
//it is called when the SNP process receives a signal SIGINT
void network_stop() {
	//add your code here
	if (overlay_conn != -1) {
		close(overlay_conn);
		overlay_conn = -1;
	}
	printf("%s Stop.\n", output);
	exit(0);
}


int main(int argc, char *argv[]) {
	printf("network layer is starting, pls wait...\n");

	//initialize global variables
	overlay_conn = -1;

	//register a signal handler which will kill the process
	signal(SIGINT, network_stop);

	//connect to overlay
	overlay_conn = connectToOverlay();
	if(overlay_conn<0) {
		printf("can't connect to ON process\n");
		exit(1);		
	}
	
	//start a thread that handles incoming packets from overlay
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//start a route update thread 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("network layer is started...\n");

	while(1) {
		sleep(60);
	}
}


