#include "trackerpeerTable.h"

tracker_peer_t* tracker_peer_table; //will eventually go into main tracker.c file
int tracker_peer_table_size;

void trackerpeerTable_Initialize(){
	tracker_peer_table = NULL;
	tracker_peer_table_size = 0;
	return;
}

void trackerpeerTable_Destroy(){
  	tracker_peer_t* node = tracker_peer_table;
	while(node != NULL){
		tracker_peer_t* temp = node;
		node = node->next;
		free(temp);
	}
	return;
}

void trackerpeerTable_Print(){
	tracker_peer_t* pt = tracker_peer_table;
  	printf("\nTRACKER PEER TABLE-------------------------------------------\n\n");
	printf("%-20s%-30s%-20s\n", "IP Address", "Most Recent Timestamp", "Socket Descriptor");
	while(pt != NULL){
		printf("%-20s%-30lu%-20d\n", pt->IP, pt->last_time_stamp, pt->sockfd);
		pt = pt->next;
	}
	printf("---------------------------------------------------------\n\n");
	return;
}

//void trackerpeerTable_UpdateTimeStamp(char *IP, time_t timestamp){
//	tracker_peer_t* check = trackerpeerTable_Exist(IP);
//	tracker_peer_t* pt = tracker_peer_table;
//	if(check == NULL){
//		return;
//	}
//	check->last_time_stamp = timestamp;
//}

tracker_peer_t *trackerpeerTable_Add(char *IP, int socket, time_t timestamp){
	tracker_peer_t *temp = trackerpeerTable_Exist(IP);
	if (temp != NULL) {
		return temp;
	}
	temp = (tracker_peer_t *) malloc(sizeof(tracker_peer_t));
	memset(temp, 0, sizeof(tracker_peer_t));

	strcpy(temp->IP, IP);
	temp->last_time_stamp = timestamp;
	temp->sockfd = socket;
	temp->next = NULL;
	if (tracker_peer_table == NULL) {
		tracker_peer_table = temp;
		tracker_peer_table_size++;
		return temp;
	}
	tracker_peer_t* pt = tracker_peer_table;
	while (pt->next != NULL) {
		pt = pt->next;
	}
	pt->next = temp;
	tracker_peer_table_size++;
	return temp;
}

void trackerpeerTable_Delete(char *IP){
	tracker_peer_t *temp, *prev;
	temp=tracker_peer_table;
	while(temp!=NULL)
	{
		if((strcmp(temp->IP, IP) == 0))
		{
			if(temp==tracker_peer_table)
			{
				tracker_peer_table=temp->next;
				close(temp->sockfd);
				free(temp);
				tracker_peer_table_size--;
				return;
			}
			else
			{
				prev->next=temp->next;
				close(temp->sockfd);
				free(temp);
				tracker_peer_table_size--;
				return;
			}
		}
		else
		{
			prev=temp;
			temp= temp->next;
		}
	}
	return;
}

tracker_peer_t *trackerpeerTable_Exist(char *IP){
	tracker_peer_t* temp = tracker_peer_table;
	while (temp != NULL){
		if(IP == NULL || strcmp(temp->IP, IP) == 0) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

//int trackerpeerTable_gettableSize(){
//	int count = 0;
//	tracker_peer_t* temp = tracker_peer_table;
//	while(temp != NULL){
//		count++;
//		temp = temp->next;
//	}
//	return count;
//}
