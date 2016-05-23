//FILE: network/network.c
//
//Description: this file implements network layer process  
//
//Date: April 29,2008



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "network.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//network layer waits this time for establishing the routing paths 
#define NETWORK_WAITTIME 60

/**************************************************************/
//delare global variables
/**************************************************************/
int overlay_conn; 			//connection to the overlay
int transport_conn;			//connection to the transport
nbr_cost_entry_t* nct;			//neighbor cost table
dv_t* dv;				//distance vector table
pthread_mutex_t* dv_mutex;		//dvtable mutex
routingtable_t* routingtable;		//routing table
pthread_mutex_t* routingtable_mutex;	//routingtable mutex

const char *output = "Network:";

/**************************************************************/
//implementation network layer functions
/**************************************************************/

//This function is used to for the SNP process to connect to the local ON process on port OVERLAY_PORT.
//TCP descriptor is returned if success, otherwise return -1.
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
//The route update packet contains this node's distance vector. 
//Broadcasting is done by set the dest_nodeID in packet header as BROADCAST_NODEID
//and use overlay_sendpkt() to send the packet out using BROADCAST_NODEID address.
void* routeupdate_daemon(void* arg) {
	//put your code here
	while (1) {
		pkt_routeupdate_t update;
		update.entryNum = topology_getNodeNum();
		int j;
		pthread_mutex_lock(dv_mutex);
		for (j = 0; j < update.entryNum; j++) {
			update.entry[j].nodeID	= dv[0].dvEntry[j].nodeID;
			update.entry[j].cost	= dv[0].dvEntry[j].cost;
		}
		pthread_mutex_unlock(dv_mutex);
		snp_pkt_t pkt;
		pkt.header.src_nodeID	= topology_getMyNodeID();
		pkt.header.dest_nodeID	= BROADCAST_NODEID;
		pkt.header.length		= sizeof(update.entryNum) + update.entryNum * sizeof(routeupdate_entry_t);
		pkt.header.type			= ROUTE_UPDATE;
		memcpy(pkt.data, &update, pkt.header.length);
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

//This thread handles incoming packets from the ON process.
//It receives packets from the ON process by calling overlay_recvpkt().
//If the packet is a SNP packet and the destination node is this node, forward the packet to the SRT process.
//If the packet is a SNP packet and the destination node is not this node, forward the packet to the next hop according to the routing table.
//If this packet is an Route Update packet, update the distance vector table and the routing table. 
void* pkthandler(void* arg) {
	//put your code here
	snp_pkt_t pkt;

	int myID = topology_getMyNodeID();
	while(overlay_recvpkt(&pkt,overlay_conn)>0) {
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
		switch (pkt.header.type) {
			case ROUTE_UPDATE: {
				//update the dvt and rt
				int i, j;
				int n = topology_getNbrNum();
				int src_nodeID = pkt.header.src_nodeID;
				pkt_routeupdate_t a;
				memcpy(&a, &pkt.data, pkt.header.length);
				pthread_mutex_lock(dv_mutex);
				for (i = 0; i < n + 1; i++) {
					if (dv[i].nodeID == src_nodeID) {
						for (j = 0; j < a.entryNum; j++) {
							//update the dvtable
							dv[i].dvEntry[j].nodeID = a.entry[j].nodeID;
							dv[i].dvEntry[j].cost	= a.entry[j].cost;
						}
						break;
					}
				}
				//find out if there's a quicker path and update the rt
				pthread_mutex_lock(routingtable_mutex);
				for (j = 0; j < a.entryNum; j++) {
					int dest_nodeID = dv[0].dvEntry[j].nodeID;
					int old_cost = dvtable_getcost(dv, myID, dest_nodeID);
					int new_cost = dvtable_getcost(dv, myID, src_nodeID) + dvtable_getcost(dv, src_nodeID, dest_nodeID);
					if (new_cost < old_cost) {
						dvtable_setcost(dv, myID, dest_nodeID, new_cost);
						routingtable_setnextnode(routingtable, dest_nodeID, src_nodeID);
						//dvtable_print(dv);
						//routingtable_print(routingtable);
					}
				}
				pthread_mutex_unlock(routingtable_mutex);
				pthread_mutex_unlock(dv_mutex);
				
				break;
			}
			case SNP: {
				if (pkt.header.dest_nodeID == myID) {
					printf("Routing: forward packge to local SRT!\n");
					forwardsegToSRT(transport_conn, pkt.header.src_nodeID, (seg_t *) &pkt.data);
				}
				else {
					//get the next hop
					pthread_mutex_lock(routingtable_mutex);
					int nextHop = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
					pthread_mutex_unlock(routingtable_mutex);
					if (nextHop != -1) {
						printf("Routing: forward packge to %d!\n", nextHop);
						overlay_sendpkt(nextHop, &pkt, overlay_conn);
					}
					else {
						printf("Routing: packet cannot forwarded to %d for now!\n", pkt.header.dest_nodeID);
					}
				}
				break;
			}
		}
	}
	printf("%s local overlay is lost! Exit...\n", output);
	pthread_exit(NULL);
}

//This function stops the SNP process. 
//It closes all the connections and frees all the dynamically allocated memory.
//It is called when the SNP process receives a signal SIGINT.
void network_stop() {
	//put your code here
	printf("%s Stop.\n", output);
    nbrcosttable_destroy(nct);
    dvtable_destroy(dv);
    routingtable_destroy(routingtable);
	if (overlay_conn != -1) {
		close(overlay_conn);
	}
	if (transport_conn!= -1) {
		close(transport_conn);
	}
}

//This function opens a port on NETWORK_PORT and waits for the TCP connection from local SRT process.
//After the local SRT process is connected, this function keeps receiving sendseg_arg_ts which contains the segments and their destination node addresses from the SRT process. The received segments are then encapsulated into packets (one segment in one packet), and sent to the next hop using overlay_sendpkt. The next hop is retrieved from routing table.
//When a local SRT process is disconnected, this function waits for the next SRT process to connect.
void waitTransport() {
	//put your code here
	int myID = topology_getMyNodeID();
	struct sockaddr_in tcpserv_addr;
	
	int tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(NETWORK_PORT);
	
	bind(tcpserv_sd, (struct sockaddr *) &tcpserv_addr, sizeof(tcpserv_addr));
	listen(tcpserv_sd, 1);
	while (1) {
		struct sockaddr_in tcpclient_addr;
		socklen_t tcpclient_addr_len = sizeof(tcpclient_addr);
		transport_conn = accept(tcpserv_sd, (struct sockaddr *) &tcpclient_addr, &tcpclient_addr_len);
		if (transport_conn < 0) {
			perror("accept");
			return;
		}
		printf("%s Local SRT connection established!\n", output);
		seg_t a;
		int nextNode;
		while (1 == getsegToSend(transport_conn, &nextNode, &a)) {
			snp_pkt_t b;
			b.header.src_nodeID = myID;
			b.header.dest_nodeID = nextNode;
			b.header.length = a.header.length + sizeof(srt_hdr_t);
			b.header.type = SNP;
			memcpy(&b.data, &a, b.header.length);
			//get the next hop
			pthread_mutex_lock(routingtable_mutex);
			int nextHop = routingtable_getnextnode(routingtable, b.header.dest_nodeID);
			pthread_mutex_unlock(routingtable_mutex);
			if (nextHop != -1) {
				printf("%s Received a pkt from local SRT! Routing: forward packge to %d!\n", output, nextHop);
				overlay_sendpkt(nextHop, &b, overlay_conn);
			}
			else {
				printf("%s Received a pkt from local SRT! Routing: packet cannot forwarded to %d for now!\n", output, b.header.dest_nodeID);
			}
		}
		if (transport_conn != -1) {
			printf("%s Local SRT is lost!\n", output);
			close(transport_conn);
			transport_conn = -1;
		}
		else {
			printf("%s Exiting...\n", output);
			return;
		}
	}
}

int main(int argc, char *argv[]) {
	printf("network layer is starting, pls wait...\n");

	//initialize global variables
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	overlay_conn = -1;
	transport_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//register a signal handler which is used to terminate the process
	signal(SIGINT, network_stop);

	//connect to local ON process 
	overlay_conn = connectToOverlay();
	if(overlay_conn<0) {
		printf("can't connect to overlay process\n");
		exit(1);		
	}
	
	//start a thread that handles incoming packets from ON process 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//start a route update thread 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("network layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(NETWORK_WAITTIME);
	routingtable_print(routingtable);

	//wait connection from SRT process
	printf("waiting for connection from SRT process\n");
	waitTransport(); 

}


