
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//This function creates a dvtable(distance vector table) dynamically.
//A distance vector table contains the n+1 entries, where n is the number of the neighbors of this node, and the rest one is for this node itself. 
//Each entry in distance vector table is a dv_t structure which contains a source node ID and an array of N dv_entry_t structures where N is the number of all the nodes in the overlay.
//Each dv_entry_t contains a destination node address the the cost from the source node to this destination node.
//The dvtable is initialized in this function.
//The link costs from this node to its neighbors are initialized using direct link cost retrived from topology.dat. 
//Other link costs are initialized to INFINITE_COST.
//The dynamically created dvtable is returned.
dv_t* dvtable_create()
{
	int n = topology_getNbrNum();
	int N = topology_getNodeNum();
	int *nbrID = topology_getNbrID();
	int *nodeID = topology_getNodeArray();
	int myID = topology_getMyNodeID();
	dv_t *a = (dv_t *) malloc(sizeof(dv_t) * (n + 1));
	int i, j;
	a[0].nodeID = myID;
	a[0].dvEntry = (dv_entry_t *) malloc(sizeof(dv_entry_t) * N);
	for (j = 0; j < N; j++) {
		a[0].dvEntry[j].nodeID	= nodeID[j];
		a[0].dvEntry[j].cost	= topology_getCost(myID, a[0].dvEntry[j].nodeID);
	}
	for (i = 1; i < n + 1; i++) {
		a[i].nodeID = nbrID[i - 1];
		a[i].dvEntry = (dv_entry_t *) malloc(sizeof(dv_entry_t) * N);
		for (j = 0; j < N; j++) {
			a[i].dvEntry[j].nodeID	= nodeID[j];
			a[i].dvEntry[j].cost	= INFINITE_COST;
		}
	}
	free(nbrID);
	free(nodeID);
	return a;
}

//This function destroys a dvtable. 
//It frees all the dynamically allocated memory for the dvtable.
void dvtable_destroy(dv_t* dvtable)
{
	int n = topology_getNbrNum();
	int i;
	for (i = 0; i < n + 1; i++) {
		free(dvtable[i].dvEntry);
	}
	free(dvtable);
}


//This function sets the link cost between two nodes in dvtable.
//If those two nodes are found in the table and the link cost is set, return 1.
//Otherwise, return -1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i, j;
	int n = topology_getNbrNum();
	int N = topology_getNodeNum();
	for (i = 0; i < n + 1; i++) {
		if (dvtable[i].nodeID == fromNodeID) {
			for (j = 0; j < N; j++) {
				if (dvtable[i].dvEntry[j].nodeID == toNodeID) {
					dvtable[i].dvEntry[j].cost = cost;
					return 1;
				}
			}
		}
	}
	return -1;
}

//This function returns the link cost between two nodes in dvtable
//If those two nodes are found in dvtable, return the link cost. 
//otherwise, return INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i, j;
	int n = topology_getNbrNum();
	int N = topology_getNodeNum();
	for (i = 0; i < n + 1; i++) {
		if (dvtable[i].nodeID == fromNodeID) {
			for (j = 0; j < N; j++) {
				if (dvtable[i].dvEntry[j].nodeID == toNodeID) {
					return dvtable[i].dvEntry[j].cost;
				}
			}
		}
	}
	return INFINITE_COST;
}

//This function prints out the contents of a dvtable.
void dvtable_print(dv_t* dvtable)
{
	printf("***************************************************\n");
	int i, j;
	int n = topology_getNbrNum();
	int N = topology_getNodeNum();
	int *nodeID = topology_getNodeArray();
	int myID = topology_getMyNodeID();
	printf("Contents of node %d's dvtable:\n", myID);
	printf("%-8s", "");
	for (j = 0; j < N; j++) {
		printf("%-8d", nodeID[j]);
	}
	printf("\n");
	for (i = 0; i < n + 1; i++) {
		printf("%-8d", dvtable[i].nodeID);
		for (j = 0; j < N; j++) {
			printf("%-8d", dvtable[i].dvEntry[j].cost);
		}
		printf("\n");
	}
	printf("***************************************************\n");
	free(nodeID);
	return;
}
