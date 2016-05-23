//FILE: overlay/neighbortable.c
//
//Description: this file the API for the neighbor table
//
//Date: May 03, 2010

#include "neighbortable.h"

//This function first creates a neighbor table dynamically. It then parses the topology/topology.dat file and fill the nodeID and nodeIP fields in all the entries, initialize conn field as -1 .
//return the created neighbor table
nbr_entry_t* nt_create()
{
	int nbrNum = topology_getNbrNum();
	nbr_entry_t *a = (nbr_entry_t *) malloc(sizeof(nbr_entry_t) * nbrNum);
	int *id = topology_getNbrID();
	in_addr_t *ip = topology_getNbrIP();
	int i;
	for (i = 0; i < nbrNum; i++) {
		a[i].nodeID = id[i];
		a[i].nodeIP = ip[i];
		a[i].conn	= -1;
	}
	free(id);
	free(ip);
	return a;
}

//This function destroys a neighbortable. It closes all the connections and frees all the dynamically allocated memory.
void nt_destroy(nbr_entry_t* nt)
{
	int i;
	int nbrNum = topology_getNbrNum();
	for (i = 0; i < nbrNum; i++) {
		if (nt[i].conn != -1)
			close(nt[i].conn);
	}
	free(nt);
}

//This function is used to assign a TCP connection to a neighbor table entry for a neighboring node. If the TCP connection is successfully assigned, return 1, otherwise return -1
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	int i;
	int nbrNum = topology_getNbrNum();
	for (i = 0; i < nbrNum; i++) {
		if (nt[i].nodeID == nodeID) {
			nt[i].conn = conn;
			return 1;
		}
	}
	return -1;
}
