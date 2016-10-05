// File pkt.c
// May 03, 2010
#define _DEFAULT_SOURCE
#include "pkt.h"

//--------------------------- Helper function -----------------------------------------------------
char* my_ip(){
	struct hostent *he;
	struct in_addr **addr_list;
	int i;

	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);

	if ( (he = gethostbyname( hostname ) ) == NULL) 
	{
		// get the host info
		herror("gethostbyname");
		return NULL;
	}
 
	addr_list = (struct in_addr **) he->h_addr_list;
	 
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		return inet_ntoa(*addr_list[i]);
	}
	 
	return NULL;
}

// ----------------------------------------------------------------------------------------------------

//
//int p2T_pkt_init(ptp_peer_t* pkt){
//	//Protocol Length
//	pkt->protocol_len = 0;
//	//packet type : register, keep alive, update file table
//	pkt->type = -1;
//	//listening port number in p2p
//	pkt->port = -1;
//	//the number of files in local file table -- optional
//	pkt->file_table_size = 0;
//	return 1;	
//}
//
//
//void destroy_pkt(ptp_peer_t* pkt){
//	free(pkt->protocol_name);
//	free(pkt->reserved);
//	free(pkt->peer_ip);
//	filenode* current = pkt->file_table;
//	filenode* del;
//	while(current!=NULL){
//		del = current;
//		current = current->pNext;
//		free(del->name);
//		free(del->newpeerip);
//		free(del);
//	}
//}
//
int p2T_pkt_set(ptp_peer_t* pkt, int protocol_length, char* protocol_name, REQUEST_TYPE type, char* reserved_data, char* peer_ip, int port, int file_table_size, filenode* file_table ){
	//Protocol Length
	pkt->protocol_len = protocol_length;
	//protocol name
	if(protocol_name!=NULL){
		if(strlen(protocol_name)>PROTOCOL_LEN){
			printf("Error assigning protocol_name. ALlocated more memory to PROTOCOL_LEN");
			return -1;
		}
		// pkt->protocol_name = calloc(strlen(protocol_name)+2, sizeof(char));
		memcpy(pkt->protocol_name,protocol_name,strlen(protocol_name));
	}

	//packet type : register, keep alive, update file table
	pkt->type = type;
	//reserved space, you could use this space for your convenient, 8 bytes by default
	if(reserved_data!=NULL){
		if(strlen(reserved_data)>RESERVED_LEN){
			printf("Reserved Data may be more than the allocated buffer. It will be truncated\n");
		}
		// pkt->reserved = calloc(RESERVED_LEN+2,sizeof(char));
		memcpy(pkt->reserved,reserved_data, RESERVED_LEN);
	}
	
	//the peer ip_address sending this packet
	if(peer_ip!=NULL){
		if(strlen(peer_ip)>IP_LEN){
			printf("peer IP address may be more than the allocated buffer. It will be truncated\n");
		}
		// pkt->peer_ip = calloc(IP_LEN+2,sizeof(char));
		memcpy(pkt->peer_ip,peer_ip,IP_LEN);
	}
	//listening port number in p2p
	pkt->port = port;
	//the number of files in local file table -- optional
	pkt->file_table_size = file_table_size;
	//file table of the client -- your own design
	pkt->file_table = file_table;

	return 1;
}


//int T2P_pkt_init(ptp_tracker_t* pkt){
//	//time interval that the peer should be sing to send alive message periodically
//	pkt->interval = 0;
//	//piece length
//	pkt->piece_len = -1;
//	//file table size = number of file nodes:
//	pkt->file_table_size = -1;
//	//the number of files in local file table -- optional
//	pkt->file_table = NULL;	
//	return 1;
//}


int sendpkt(int sockfd, ptp_peer_t* pkt)
{
	if (sendfunc(sockfd, (void *) pkt, sizeof(ptp_peer_t)) < 0) {
		return -1;
	}
	if (pkt->type == FILE_UPDATE)
		return fileTable_Send(sockfd);
	return 1;
}

int recvpkt(int sockfd, ptp_peer_t* pkt)
{
	if (recvfunc(sockfd, (void *) pkt, sizeof(ptp_peer_t)) < 0) {
		return -1;
	} 
	if (pkt->type == FILE_UPDATE) {
		return fileTable_Receive(sockfd, &pkt->file_table);
	}
	return 1;
}

