#ifndef PEERPEERTABLE_H 
#define PEERPEERTABLE_H

#include "../common/fileTable.h"

typedef struct _peer_side_peer_t {
	char IP[IP_LEN];
	char filename[FILE_NAME_LEN];
	time_t last_timestamp;
	int sockfd;
	struct _peer_side_peer_t *next;
	char *addr;
	int start;
	int end;
} peer_peer_t;

void peerpeerTable_Initialize();

void peerpeerTable_Destroy();

peer_peer_t *peerpeerTable_Add(filenode *f);

void peerpeerTable_Delete(peer_peer_t *t);

void peerpeerTable_Print();

peer_peer_t *peerpeerTable_Exist(peer_peer_t *head, char *name);

#endif

