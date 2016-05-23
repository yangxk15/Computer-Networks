

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//This is the hash function used the by the routing table
//It takes the hash key - destination node ID as input, 
//and returns the hash value - slot number for this destination node ID.
//
//You can copy makehash() implementation below directly to routingtable.c:
//int makehash(int node) {
//	return node%MAX_ROUTINGTABLE_ENTRIES;
//}
//
int makehash(int node)
{
	return node % MAX_ROUTINGTABLE_SLOTS;
}

//This function creates a routing table dynamically.
//All the entries in the table are initialized to NULL pointers.
//Then for all the neighbors with a direct link, create a routing entry using the neighbor itself as the next hop node, and insert this routing entry into the routing table. 
//The dynamically created routing table structure is returned.
routingtable_t* routingtable_create()
{
	routingtable_t *a = (routingtable_t *) malloc(sizeof(routingtable_t));
	int i;
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		a->hash[i] = NULL;
	}
	int *nbrID = topology_getNbrID();
	int nbrNum = topology_getNbrNum();
	for (i = 0; i < nbrNum; i++) {
		routingtable_setnextnode(a, nbrID[i], nbrID[i]);
	}
	free(nbrID);
	return a;
}

//recursive free helper function
void freefunc(routingtable_entry_t *t) {
	if (t == NULL)
		return;
	freefunc(t->next);
	free(t);
}
//This funtion destroys a routing table. 
//All dynamically allocated data structures for this routing table are freed.
void routingtable_destroy(routingtable_t* routingtable)
{
	int i;
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		freefunc(routingtable->hash[i]);
	}
	return;
}

//This function updates the routing table using the given destination node ID and next hop's node ID.
//If the routing entry for the given destination already exists, update the existing routing entry.
//If the routing entry of the given destination is not there, add one with the given next node ID.
//Each slot in routing table contains a linked list of routing entries due to conflicting hash keys (differnt hash keys (destination node ID here) may have same hash values (slot entry number here)).
//To add an routing entry to the hash table:
//First use the hash function makehash() to get the slot number in which this routing entry should be stored. 
//Then append the routing entry to the linked list in that slot.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int value = makehash(destNodeID);
	if (-1 == routingtable_getnextnode(routingtable, destNodeID)) {
		routingtable_entry_t *t = (routingtable_entry_t *) malloc(sizeof(routingtable_entry_t));
		t->destNodeID = destNodeID;
		t->nextNodeID = nextNodeID;
		t->next = NULL;
		if (routingtable->hash[value] == NULL) {
			routingtable->hash[value] = t;
		}
		else {
			routingtable_entry_t *cur = routingtable->hash[value];
			while (cur->next != NULL) {
				cur = cur->next;
			}
			cur->next = t;
		}
	}
	else {
		routingtable_entry_t *cur = routingtable->hash[value];
		while (cur->destNodeID != destNodeID) {
			cur = cur->next;
		}
		cur->nextNodeID = nextNodeID;
	}
	return;
}

//This function looks up the destNodeID in the routing table.
//Since routing table is a hash table, this opeartion has O(1) time complexity.
//To find a routing entry for a destination node, you should first use the hash function makehash() to get the slot number and then go through the linked list in that slot to search the routing entry.
//If the destNodeID is found, return the nextNodeID for this destination node.
//If the destNodeID is not found, return -1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int value = makehash(destNodeID);
	routingtable_entry_t *cur = routingtable->hash[value];
	while (cur != NULL) {
		if (cur->destNodeID == destNodeID) {
			return cur->nextNodeID;
		}
		cur = cur->next;
	}
	return -1;
}

//This function prints out the contents of the routing table
void routingtable_print(routingtable_t* routingtable)
{
	int myID = topology_getMyNodeID();
	printf("***************************************************\n");
	printf("Contents of node %d's routing table:\n", myID);
	int i;
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		routingtable_entry_t *cur = routingtable->hash[i];
		while (cur != NULL) {
			printf("destNodeID: %10d\tnextNodeID: %10d\n", cur->destNodeID, cur->nextNodeID);
			cur = cur->next;
		}
	}
	
	printf("***************************************************\n");
	return;
}














