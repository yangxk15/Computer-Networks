//FILE: topology/topology.c
//
//Description: this file implements some helper functions used to parse 
//the topology file 
//
//Date: May 3,2010

#include "topology.h"

const char *filename	= "topology/topology.dat";

//this function return ip of the given hostname
in_addr_t getipfromname(char* hostname) 
{
	struct sockaddr_in hostaddr;
	struct hostent *hostInfo;
	hostInfo = gethostbyname(hostname);
	memcpy((char *) &hostaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);	

	return hostaddr.sin_addr.s_addr;
}
//this function returns node ID of the given hostname
//the node ID is an integer of the last 8 digit of the node's IP address
//for example, a node with IP address 202.120.92.3 will have node ID 3
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromname(char* hostname) 
{
	struct sockaddr_in hostaddr;
	struct hostent *hostInfo;
	hostInfo = gethostbyname(hostname);
	memcpy((char *) &hostaddr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);	

	return topology_getNodeIDfromip(&hostaddr.sin_addr);
}

//this function returns node ID from the given IP address
//if the node ID can't be retrieved, return -1
int topology_getNodeIDfromip(struct in_addr* addr)
{
	return atoi(strrchr(inet_ntoa(*addr), '.') + 1);
}

//this function returns my node ID
//if my node ID can't be retrieved, return -1
int topology_getMyNodeID()
{
	struct ifaddrs *ifaddr, *ifa;
	char ip[TNAMEBUF] = {0};

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
    }
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && strncmp(ifa->ifa_name, "e", 1) == 0) {
	        struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
	        strcpy(ip, inet_ntoa(pAddr->sin_addr));
			break;
		}
	}

	freeifaddrs(ifaddr);

	return topology_getNodeIDfromname(ip);
}

int add(char *names[], int count, const char *s)
{
	int i;
	for (i = 0; i < count; i++) {
		if (strcmp(names[i], s) == 0) {
			return count;
		}
	}
	strcpy(names[count++], s);
	return count;
}
//this functions parses the topology information stored in topology.dat
//returns the number of total nodes in the overlay 
int topology_getNodeNum()
{ 
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];
	int i;
	int count = 0;
	char *names[MAX_NODE_NUM];
	for (i = 0; i < MAX_NODE_NUM; i++) {
		names[i] = (char *) malloc(sizeof(char) * TNAMEBUF);
	}

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		count = add(names, count, host1);
		count = add(names, count, host2);
	}

	for (i = 0; i < MAX_NODE_NUM; i++) {
		free(names[i]);
	}

	fclose(file);
	return count;
}

//this functions parses the topology information stored in topology.dat
//returns the number of neighbors
/* assume that same pair will not appear twice */
int topology_getNbrNum()
{
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];
	int count = 0;
	int id = topology_getMyNodeID();

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		int id1 = topology_getNodeIDfromname(host1);
		int id2 = topology_getNodeIDfromname(host2);
		if (id == id1 || id == id2) {
			count++;
		}
	}

	fclose(file);
	return count;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the nodes' IDs in the overlay network  
int *topology_getNodeArray()
{ 
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];
	int i;
	int count = 0;
	char *names[MAX_NODE_NUM];
	for (i = 0; i < MAX_NODE_NUM; i++) {
		names[i] = (char *) malloc(sizeof(char) * TNAMEBUF);
	}

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		count = add(names, count, host1);
		count = add(names, count, host2);
	}
	int *a = (int *) malloc(sizeof(int) * count);
	for (i = 0; i < count; i++) {
		a[i] = topology_getNodeIDfromname(names[i]);
	}

	for (i = 0; i < MAX_NODE_NUM; i++) {
		free(names[i]);
	}

	fclose(file);
	return a;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IDs  
int *topology_getNbrID()
{
	int i = 0;
	int *a = (int *) malloc(sizeof(int) * topology_getNbrNum());
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];
	int id = topology_getMyNodeID();

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		int id1 = topology_getNodeIDfromname(host1);
		int id2 = topology_getNodeIDfromname(host2);
		if (id == id1) {
			a[i++] = id2;
		}
		else if (id == id2) {
			a[i++] = id1;
		}
	}

	fclose(file);
	return a;
}

//this functions parses the topology information stored in topology.dat
//returns a dynamically allocated array which contains all the neighbors'IPs  
in_addr_t *topology_getNbrIP()
{
	int i = 0;
	in_addr_t *a = (in_addr_t *) malloc(sizeof(in_addr_t) * topology_getNbrNum());
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];
	int id = topology_getMyNodeID();

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		int id1 = topology_getNodeIDfromname(host1);
		int id2 = topology_getNodeIDfromname(host2);
		if (id == id1) {
			a[i++] = getipfromname(host2);
		}
		else if (id == id2) {
			a[i++] = getipfromname(host1);
		}
	}

	fclose(file);
	return a;
}

//this functions parses the topology information stored in topology.dat
//returns the cost of the direct link between the two given nodes 
//if no direct link between the two given nodes, INFINITE_COST is returned
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	FILE* file = fopen(filename, "r");
    char line[TLINEBUF];

	while (fgets(line, sizeof(line), file)) {
		char host1[TNAMEBUF] = {0};
		char host2[TNAMEBUF] = {0};
		int distance;
		sscanf(line, "%s %s %d", host1, host2, &distance);
		int id1 = topology_getNodeIDfromname(host1);
		int id2 = topology_getNodeIDfromname(host2);
		if ((id1 == fromNodeID && id2 == toNodeID) || (id1 == toNodeID && id2 == fromNodeID)) {
			fclose(file);
			return distance;
		}
	}

	fclose(file);
	return INFINITE_COST;
}
