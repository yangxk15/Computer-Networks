#include "peerpeerTable.h"

peer_peer_t *peer_peer_table;
int peer_peer_table_size;

void peerpeerTable_Initialize()
{
	peer_peer_table = NULL; 
	peer_peer_table_size = 0;
}

void peerpeerTable_Destroy()
{
	peer_peer_t *t = peer_peer_table;
	while (t != NULL) {
		peer_peer_t *tmp = t;
		t = t->next;
		peerpeerTable_Delete(tmp);
	}
	peer_peer_table_size = 0;
}

peer_peer_t *peerpeerTable_Add(filenode *f)
{
	// initialize this new node with input information
	peer_peer_t *t = (peer_peer_t *) malloc(sizeof(peer_peer_t));
	memset(t, 0, sizeof(peer_peer_t));
	memcpy(t->IP, f->peerIP, IP_LEN);
	memcpy(t->filename, f->filename, FILE_NAME_LEN);
	t->last_timestamp = f->timestamp;

	struct sockaddr_in server_addr;
	server_addr.sin_family		= AF_INET;	
	server_addr.sin_addr.s_addr = inet_addr(t->IP);
	server_addr.sin_port		= htons(PEER_PORT);

	assert((t->sockfd = socket(AF_INET, SOCK_STREAM, 0)) >= 0);
	assert(connect(t->sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == 0); 

	t->next = NULL;

	// add this new node to the table
	if (peer_peer_table == NULL) {
		peer_peer_table = t; 
	}
	else {
		peer_peer_t *p = peer_peer_table;
		while (p->next != NULL) {
			p = p->next;
		}
		p->next = t;
	}
	peer_peer_table_size++;

	return t;
}

void peerpeerTable_Delete(peer_peer_t *t)
{
	// close the sockfd
	// here we assume that sockfd is a valid connection
	close(t->sockfd);

	// delete the input node from the table
	if (peer_peer_table == t) {
		peer_peer_table = t->next;
	} 
	else {
		peer_peer_t *p = peer_peer_table;
		while (p->next != t) {
			p = p->next;
		}
		p->next = t->next;
	}
	free(t);
	peer_peer_table_size--;
}

void peerpeerTable_Print()
{
	peer_peer_t* pt = peer_peer_table;
  	printf("\nPEER PEER TABLE-------------------------------------------\n\n");
	printf("%-20s%-10s%-10s%-20s%-10s\n", "File Name", "Start", "End", "IP Address", "Sockfd");
	while(pt != NULL){
		printf("%-20s%-10d%-10d%-20s%-10d\n", pt->filename, pt->start, pt->end, pt->IP, pt->sockfd);
		pt = pt->next;
	}
	printf("---------------------------------------------------------\n\n");
	return;
}

peer_peer_t *peerpeerTable_Exist(peer_peer_t *head, char *name)
{
	peer_peer_t *p = head;
	while (p != NULL) {
		if (strcmp(p->filename, name) == 0)
			break;
		p = p->next;
	}
	return p;
}







