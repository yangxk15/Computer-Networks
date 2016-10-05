#ifndef FILETABLE_H 
#define FILETABLE_H

#include "constants.h"

typedef struct node{
	int filesize;
	char filename[FILE_NAME_LEN];
	time_t timestamp;
	struct node *next;
	char peerIP[IP_LEN];
}filenode;

void fileTable_Initialize(); 

void fileTable_Destroy(filenode *node); 

int fileTable_Send(int sockfd); 

int fileTable_Receive(int sockfd, filenode **fp); 

void fileTable_Print(); 

//void fileTable_Update(filenode* peer_ft);

void fileTable_Add(filenode* target, char *IP);

void fileTable_Delete(const char * filename, char * IP);

filenode* fileTable_Exist(filenode* head, char * filename, char * IP);

//int getpeerNum(char * filename);
//
//int gettableSize();

//time_t fileTable_Latest(filenode *head, char * filename);

// sendfunc interface
int sendfunc(int conn, void *pkt, int length);

// recvfunc interface
int recvfunc(int conn, void *pkt, int length);
#endif

