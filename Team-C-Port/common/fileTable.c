//The Global Tacker File Table Keeping a Union of all the File Tables
#include "fileTable.h"

#define PKTSTART1 0
#define PKTSTART2 1
#define PKTRECV 2
#define PKTSTOP1 3

filenode* file_table; 
int file_table_size;

void fileTable_Initialize()
{
	file_table = NULL; 
	file_table_size = 0;
}

//Frees all memory dynamically allocated to the tracker file Table
void fileTable_Destroy(filenode *node)
{
	while(node != NULL) {
		filenode* temp = node;
		node = node->next;
		free(temp);
	}
  	return;
}

//sends is file table to a particular peer/tracker depending on that peers socket ID
int fileTable_Send(int sockfd)
{
	if (sendfunc(sockfd, (void *) &file_table_size, sizeof(file_table_size)) < 0) {
		printf("Error Sending File Table Count\n");
		return -1;
	}

	filenode* temp = file_table;
	while (temp != NULL) {
		if (sendfunc(sockfd, (void *) temp, sizeof(filenode)) < 0) {
			printf("Error Sending File Table\n");
			return -1;
		}
		temp = temp->next;
	}
	return 1;
}

//Function to be able to recieve a file table (will basically be used by th peers)
int fileTable_Receive(int sockfd, filenode **fp)
{
	int count;
	if (recvfunc(sockfd, (void *) &count, sizeof(count)) < 0) {
		printf("Error Receiving File Table Count\n");
		return -1;
   	}

	if (count == 0) {
		*fp = NULL;
		return 1;
	}

	*fp = (filenode *) malloc(sizeof(filenode));
	if (recvfunc(sockfd, (void *) *fp, sizeof(filenode)) < 0) {
		printf("Error Receiving File Table\n");
		return -1;
	}

	filenode *temp = *fp;
	int j;
	for(j = 1; j < count; j++){
		temp->next = (filenode *) malloc(sizeof(filenode));
		temp = temp->next;
		if (recvfunc(sockfd, (void *) temp, sizeof(filenode)) < 0) {
			printf("Error Receiving File Table\n");
			return -1;
		}
	}

	return 1;
}

//prints file table. 
void fileTable_Print(filenode *ft)
{
  	printf("\nFILE TABLE-------------------------------------------\n\n");
	printf("%-25s%-20s%-20s%-20s\n", "File Name", "File Size", "TimeStamp", "Hosting Peer");
	while(ft != NULL){
		printf("%-25s%-20d%-20lu%-20s\n", ft->filename, ft->filesize, ft->timestamp, ft->peerIP);
		ft = ft->next;
	}
	printf("---------------------------------------------------------\n\n");
  	return;
}

//Update the file table "ft "acording to the "target" filenode's filename and IP
//void fileTable_Update(filenode* target)
//{
//	filenode* check = fileTable_Exist(file_table, target->filename, target->peerIP);
//	if(check != NULL){
//		check->filesize = target->filesize;
//		check->timestamp = target->timestamp;
//		filenode* ft = file_table;
//		while (ft != NULL){
//			if((strcmp(ft->filename, target->filename) == 0) && (strcmp(ft->peerIP, target->peerIP) == 0)){
//				ft->filesize = target->filesize;
//				ft->timestamp = target->timestamp;
//			}
//			ft = ft->next;
//		}
//	}
//	return;
//}

//Add the "target" filenode to the file table "ft"
void fileTable_Add(filenode* target, char *IP)
{
	filenode* temp = (filenode *) malloc(sizeof(filenode));
	memcpy(temp, target, sizeof(filenode));
	memset(temp->peerIP, 0, IP_LEN);
	strcpy(temp->peerIP, IP);
	temp->next = NULL;

//	temp->filesize = target->filesize;
//	strcpy(temp->filename, target->filename);
//	temp->timestamp = target->timestamp;
//	strcpy(temp->peerIP, target->peerIP);

	if (file_table == NULL) {
		file_table = temp;
	}
	else {	
		filenode* ft = file_table;
		while (ft->next != NULL){
			ft = ft->next;
		}
		ft->next = temp;
	}
	file_table_size++;

	return;
}

//deleted all file nodes with the given filename from the File Table
void fileTable_Delete(const char * filename, char * IP)
{
	filenode *temp, *prev;
	temp = file_table;
	while (temp != NULL) {
		if((filename == NULL || strcmp(temp->filename, filename) == 0) && (IP == NULL || strcmp(temp->peerIP, IP) == 0))
		{
			if(temp == file_table)
			{
				file_table = temp->next;
				free(temp);
				temp = file_table;
			}
			else
			{
				prev->next = temp->next;
				free(temp);
				temp = prev->next;
			}
			file_table_size--;
		}
		else
		{
			prev = temp;
			temp = temp->next;
		}
	}
	return;
}

//this function checks to see if a certain file  with the given peer IP exists in the File Table.
//returns the file node which has that file
filenode* fileTable_Exist(filenode* head, char * filename, char * IP)
{
	//int flag = 0;
	filenode* temp = head;
	while (temp != NULL){
		if((strcmp(temp->filename, filename) == 0) && (IP == NULL || (strcmp(temp->peerIP, IP) == 0))){
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

//gets the total number of peers who have the file with the given "filename" in the 
//Tracker File Table denoted by the given "filenode* head"
//int getpeerNum(char * filename){
//	int count = 0;
//	filenode* temp = file_table;
//	while (temp != NULL){
//		if(strcmp(temp->filename, filename) == 0){
//			count++;
//		}
//		temp = temp->next;
//	}
//	return count;
//}

//gets the size of the table whoz head is given as the arguement
//int gettableSize(){
//	return file_table_size;
//}

//time_t fileTable_Latest(filenode *head, char * filename)
//{
//	filenode* temp = head;
//	time_t most_recent = 0;
//	while (temp != NULL){
//		if(strcmp(temp->filename, filename) == 0 && temp->timestamp > most_recent){
//			most_recent = temp->timestamp;
//		}
//		temp = temp->next;
//	}
//	return most_recent;
//}

// sendfunc interface
int sendfunc(int conn, void *pkt, int length)
{
	char bufstart[2] = "!&";
	char bufend[2] = "!#";
	
	if (send(conn, bufstart, 2, 0) < 0) {
		return -1;
	}
	if(send(conn, pkt, length, 0) < 0) {
		return -1;
	}
	if(send(conn, bufend, 2, 0) < 0) {
		return -1;
	}
	return length;
}

// recvfunc interface
int recvfunc(int conn, void *pkt, int length)
{
	char *buf = (char *) malloc(sizeof(char) * (length + 2));
	char c;
	int idx = 0;

	int state = PKTSTART1; 
	while (recv(conn, &c, 1, 0) > 0) {
		switch(state) {
			case PKTSTART1:
 				if(c == '!')
					state = PKTSTART2;
				break;
			case PKTSTART2:
				if(c == '&') 
					state = PKTRECV;
				else
					state = PKTSTART1;
				break;
			case PKTRECV:
				if(c == '!') {
					buf[idx] = c;
					idx++;
					state = PKTSTOP1;
				}
				else {
					buf[idx] = c;
					idx++;
				}
				break;
			case PKTSTOP1:
				if(c == '#') {
					buf[idx] = c;
					if (idx - 1 < length) {
						idx++;
						state = PKTRECV;
						break;
					}
					memcpy(pkt, buf, idx - 1);
					free(buf);
					return idx - 1;
				}
				else if(c == '!') {
					buf[idx] = c;
					idx++;
				}
				else {
					buf[idx] = c;
					idx++;	
					state = PKTRECV;
				}
				break;
			default:
				break;
	
		}
	}
	free(buf);
	return -1;
}
