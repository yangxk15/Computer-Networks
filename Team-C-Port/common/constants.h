//constant.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#define EVENT_SIZE  	(sizeof (struct inotify_event))
#define BUF_LEN	 	(1024 * (EVENT_SIZE + 16))
#define PROTOCOL_LEN	256
#define IP_LEN		256
#define FILE_NAME_LEN	256
#define RESERVED_LEN	8

//#define TRACKER_IP	"192.168.43.39"		//raspberry
//#define TRACKER_IP	"129.170.213.32"	//bear
//#define TRACKER_IP	"129.170.213.102"	//bear
#define TRACKER_IP	"129.170.213.3"		//pie

#define TRACKER_PORT 	1527
#define PEER_PORT	2715

//wait for 20 secends, if the peer didn't send the alive message, remove it.
#define MONITOR_ALIVE_INTERVAL 20
//The max peer numbe is 10.
#define MAX_PEER 10
//The monitor alive period.
#define DEAD_TIMEOUT 60
//This shall be all the same for ptp_tracker_t pkt. which may need to set in common.h
#define ALIVE_INTERVAL 5

#define PIECE_LENGTH 	262144
#define MAX_DATA_LEN	4096

#define MIN(a ,b) a < b ? a : b
#define MAX(a, b) a > b ? a : b

typedef enum {
	REGISTER,
	KEEP_ALIVE,
	FILE_UPDATE
} REQUEST_TYPE;

#endif

