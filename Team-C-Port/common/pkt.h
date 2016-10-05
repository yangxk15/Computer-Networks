#ifndef PKT_H
#define PKT_H
#include "fileTable.h"	

//The packet data structure sending from peer to tracker
typedef struct segment_peer {
  	//Protocol Length
	int protocol_len;
	//protocol name 
	char protocol_name[PROTOCOL_LEN + 1];
	//packet type : register, keep alive, update file table
	int type;
	//reserved space, you could use this space for your convenient, 8 bytes by default
	char reserved[RESERVED_LEN];
	//the peer ip_address sending this packet
	char peer_ip[IP_LEN];
	//listening port number in p2p
	int port;
	//the number of files in local file table -- optional
	int file_table_size;
	//file table of the client -- your own design
	filenode *file_table;
} ptp_peer_t;

//The packet data structure sending from tracker to peer:

typedef struct segment_tracker {
  	//time interval that the peer should sending alive messages periodically
	int interval;
	//piece length: 
	int piece_len;
	//packet type : register, keep alive, update file table
	int file_table_size;
	//file table of the tracker -- your own design
	filenode file_table;
} ptp_tracker_t;


char *my_ip();

//Initializes the peer-to-tracker pkts
//int p2T_pkt_init(ptp_peer_t* pkt);
//void destroy_pkt(ptp_peer_t* pkt);

//Initialize the pkts to certain values as required
int p2T_pkt_set(ptp_peer_t* pkt, int protocl_length, char* protocol_name, REQUEST_TYPE type, char* reserved_data, char* peer_ip, int port, int file_table_size, filenode* file_table );

//send and recv pkt
int sendpkt(int sockfd, ptp_peer_t* pkt);
int recvpkt(int sockfd, ptp_peer_t* pkt);

#endif

