#ifndef TRACKERPEERTABLE_H 
#define TRACKERPEERTABLE_H

#include "../common/constants.h"

typedef struct tracker_side_peer_t{
	char IP[IP_LEN];
	time_t last_time_stamp;
	int sockfd;
	struct tracker_side_peer_t *next;
}tracker_peer_t;

void trackerpeerTable_Initialize(); 

void trackerpeerTable_Destroy(); 

void trackerpeerTable_Print(); 

//void trackerpeerTable_UpdateTimeStamp(char *IP, time_t timestamp);

tracker_peer_t *trackerpeerTable_Add(char *IP, int socket, time_t timestamp);

void trackerpeerTable_Delete(char *IP);

tracker_peer_t *trackerpeerTable_Exist(char *IP);

//int trackerpeerTable_gettableSize();



#endif

