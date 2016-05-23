
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//This function creates a neighbor cost table dynamically 
//and initialize the table with all its neighbors' node IDs and direct link costs.
//The neighbors' node IDs and direct link costs are retrieved from topology.dat file. 
nbr_cost_entry_t* nbrcosttable_create()
{
	int myID = topology_getMyNodeID();
	int *nbrID = topology_getNbrID();
	int nbrNum = topology_getNbrNum();
	nbr_cost_entry_t *a = (nbr_cost_entry_t *) malloc(sizeof(nbr_cost_entry_t) * nbrNum);
	int i;
	for (i = 0; i < nbrNum; i++) {
		a[i].nodeID = nbrID[i];
		a[i].cost	= topology_getCost(myID, a[i].nodeID);
	}
	free(nbrID);
	return a;
}

//This function destroys a neighbor cost table. 
//It frees all the dynamically allocated memory for the neighbor cost table.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	free(nct);
	return;
}

//This function is used to get the direct link cost from neighbor.
//The direct link cost is returned if the neighbor is found in the table.
//INFINITE_COST is returned if the node is not found in the table.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i;
	int nbrNum = topology_getNbrNum();
	for (i = 0; i < nbrNum; i++) {
		if (nct[i].nodeID == nodeID) {
			return nct[i].cost;
		}
	}
	return INFINITE_COST;
}

//This function prints out the contents of a neighbor cost table.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	int myID = topology_getMyNodeID();
	int nbrNum = topology_getNbrNum();
	printf("***************************************************\n");
	printf("Contents of node %d's neighbor cost table:\n", myID);
	int i;
	for (i = 0; i < nbrNum; i++) {
		printf("Node ID: %10d\tCost: %10d\n", nct[i].nodeID, nct[i].cost);
	}
	printf("***************************************************\n");
}
