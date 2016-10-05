#include "tracker.h"


int tracker_socket;
extern tracker_peer_t *tracker_peer_table;
extern filenode *file_table;

pthread_mutex_t *tracker_file_table_mutex;
pthread_mutex_t *tracker_peer_table_mutex;

const char *tracker_trace = "Tracker:";

//need a filebroadcast function.

void broadcast_fileChange(){
	//	ptp_tracker_t *broadcastpkt = (ptp_tracker_t*) malloc (sizeof(ptp_tracker_t));
	//	int tfsize = trackerFtable_getsize(tracFileTable);
	//
	//
	//	tracker_peer_t *tracPeerHead = tracker_peer_table;
	//
	//	while(tracPeerHead != NULL){
	//		T2P_send(broadcastpkt, tracPeerHead->sockfd);
	//		tracPeerHead = tracPeerHead->next;
	//	}
	//	free(broadcastpkt);
   // broadcastfileTable(tracker_peer_table, tracFileTable);
	
	
	printf("%s Broadcast the global file table!\n", tracker_trace);
	tracker_peer_t *p = tracker_peer_table;
	
	pthread_mutex_lock(tracker_peer_table_mutex);
	while (p != NULL) {
		tracker_peer_t *tmp = p;
		p = p->next;
		if (fileTable_Send(tmp->sockfd) < 0) {
			printf("%s %s is down!\n", tracker_trace, tmp->IP);
			trackerpeerTable_Delete(tmp->IP);
		}
	}
	
	pthread_mutex_unlock(tracker_peer_table_mutex);
}


void* monitor_alive(void *arg){
	
	while(1){
		//sleep for MONITOR_ALIVE_INTERVAL to check periodly.
		sleep(MONITOR_ALIVE_INTERVAL);
		pthread_mutex_lock(tracker_peer_table_mutex);
		//Set a pointer tracker_peer_tp to traverse the whole trackerpeertable.
		trackerpeerTable_Print();
		tracker_peer_t *tracker_peer_tp;
		tracker_peer_tp = tracker_peer_table;
		
		
		time_t currTime;
		time(&currTime);
		//check all peer's timestamp.
		
		while(tracker_peer_tp!=NULL){
			tracker_peer_t *p = tracker_peer_tp;
			tracker_peer_tp = tracker_peer_tp->next;
			/*The peer is dead.*/
			if(difftime(currTime, p->last_time_stamp) > DEAD_TIMEOUT)//remove dead Peer and delete all files.
			{
				printf("%s monitor_alive(): peer %s is dead.\n", tracker_trace, p->IP);
				pthread_mutex_lock(tracker_file_table_mutex);
				fileTable_Delete(NULL, p->IP);
				pthread_mutex_lock(tracker_file_table_mutex);
				// remove peer from tracker peer table.
				trackerpeerTable_Delete(p->IP);
				
			}
		}
		pthread_mutex_unlock(tracker_peer_table_mutex);
	}
	
	pthread_exit(NULL);
	
}


void* handshake(void *arg){
	//Peer sends a pkt to tracker for the first time, thus tracker has to register this peer.
	ptp_peer_t handshk_pkt;
	memset(&handshk_pkt,0,sizeof(ptp_peer_t));
	
	tracker_peer_t *p = (tracker_peer_t *) arg;
	int broadflag;
	/*recv>0*/
	while (recvpkt(p->sockfd, &handshk_pkt) > 0) {
		time_t currTime;
		switch (handshk_pkt.type) {
			case REGISTER:
			{
				printf("%s handshake(): During handshake stage, it shall not receive any REGISTER pkt.\n", tracker_trace);
				break;
			}
			case KEEP_ALIVE:
			{
				//update the timestamp of peernode.
				
				pthread_mutex_lock(tracker_peer_table_mutex);
				time(&currTime);
				p->last_time_stamp = currTime;
				printf("%s handshake(): Receive KEER_ALIVE message from %s\n", tracker_trace, p->IP);
				pthread_mutex_unlock(tracker_peer_table_mutex);
				
				break;
			}
			case FILE_UPDATE:
				//update the file table.

			{
				
				printf("%s handshake(): Receive FILE_UPDATE message from %s\n", tracker_trace, p->IP);
				// pthread_mutex();
				pthread_mutex_lock(tracker_file_table_mutex);
				time(&currTime);
				printf("%s handshake(): The received file table from %s is: (%lu)\n", tracker_trace, p->IP, currTime);
				fileTable_Print(handshk_pkt.file_table);
				broadflag = -1;
				filenode* peerfileThead =  handshk_pkt.file_table;
				
				//1.: Check the peer File tabel first.
				
				while (peerfileThead!=NULL) {
					
					if(peerfileThead->timestamp == 0){
						peerfileThead->timestamp = currTime;
					}
					//1.1: if this file doesn't exist in global file table, then we add this file to the GLOBAL file table.
					
					//1.2 : if this file exists in global file table, check whether ip belongs to the same peer.
					
					filenode* checkfilename = fileTable_Exist(file_table, peerfileThead->filename, NULL);
					// 1.2.1: If there is no such name in global file table, the tracker will add this file anyway.
					if (checkfilename == NULL) {
						fileTable_Add(peerfileThead, peerfileThead->peerIP);
						broadflag = 1;
					}
					// 1.2.2: If there is a file which belongs to.
					else{
						if (checkfilename->timestamp < peerfileThead->timestamp) {
							fileTable_Delete(peerfileThead->filename, NULL);
							fileTable_Add(peerfileThead, peerfileThead->peerIP);
							broadflag = 1;
						}
						else {
							if (fileTable_Exist(file_table, peerfileThead->filename,peerfileThead->peerIP) == NULL) {
								fileTable_Add(peerfileThead, peerfileThead->peerIP);
								broadflag = 1;
							}
						}
				//		filenode* checkglobal = ;
				//		if (checkglobal == NULL || checkglobal->timestamp < peerfileThead->timestamp) {
				//			// do nothing
				//			fileTable_Delete(peerfileThead->filename, NULL);
				//			fileTable_Add(peerfileThead, peerfileThead->peerIP);
				//			broadflag = 1;
				//		}
					}
					peerfileThead = peerfileThead->next;
					
				}
				// 2.: Check GLOBAL file table.
				
				filenode* globalfileThead = file_table;
				
				while(globalfileThead!=NULL){
					filenode* checkpeer = fileTable_Exist(handshk_pkt.file_table, globalfileThead->filename, NULL);
					// If it is the peer who wants to delete this file, GLOBAL tracker file table will delete this node and broadcast this deletion.
					if(checkpeer == NULL) {
						char tmpname[FILE_NAME_LEN] = {0};
						memcpy(tmpname, globalfileThead->filename, FILE_NAME_LEN);
						fileTable_Delete(tmpname, NULL);
						broadflag = 1;
						break;
					}
					globalfileThead = globalfileThead->next;
				}
				
				printf("%s handshake(): Updated tracker file table is:\n", tracker_trace);
				fileTable_Print(file_table);

				//BroadCast changes to all alive peers.
				if(broadflag == 1){
					broadcast_fileChange();
				}

				fileTable_Destroy(handshk_pkt.file_table);
				pthread_mutex_unlock(tracker_file_table_mutex);
				break;
			}
				
			default:
				break;
		}
		// pthread_mutex();

	}

	pthread_mutex_lock(tracker_peer_table_mutex);
	printf("%s %s is down!\n", tracker_trace, p->IP);
	trackerpeerTable_Delete(p->IP);
	pthread_mutex_unlock(tracker_peer_table_mutex);
	return NULL;
}

int tracker_init (int port){
	int track_socket_desc;
	
	struct sockaddr_in tracker;
	bzero(&tracker,sizeof(tracker));
	
	//1. initialize tracker and peer.
	tracker.sin_family = AF_INET;
	tracker.sin_port = htons(port);
	tracker.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//2. Create TCP socket desc.
	track_socket_desc = socket(AF_INET,SOCK_STREAM, 0);
	if(track_socket_desc< 0){
		printf("%s tracker_init(): Cannot create tracker_socket_desc\n", tracker_trace);
		return -1 ;
	}
	int yes = 1;
	if (setsockopt(track_socket_desc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return -1;
	}
	//3. bind tracker_socket.
	if(bind(track_socket_desc,(struct sockaddr *)&tracker,sizeof(tracker))<0){
		printf("%s tracker_init(): Cannot bind tracker.\n", tracker_trace);
		return -1;
	}
	
	//4. listen to incoming connections.
	if(listen(track_socket_desc , MAX_PEER)<0){
		printf("%s tracker_init(): Cannot listen.\n", tracker_trace);
		return -1;
	}
	//5. return tracker_socket
	return track_socket_desc;
}

void tracker_stop(){
	
	printf("%s stop!\n", tracker_trace);
	//1. destroy peer_table
	
	pthread_mutex_lock(tracker_peer_table_mutex);
	trackerpeerTable_Destroy();
	pthread_mutex_unlock(tracker_peer_table_mutex);
	printf("%s trakcer_stop(): Tracker peertable destoryed.\n", tracker_trace);
	
	//2. destroy file_table;
	pthread_mutex_lock(tracker_file_table_mutex);
	fileTable_Destroy(file_table);
	pthread_mutex_unlock(tracker_file_table_mutex);
	printf("%s trakcer_stop(): Tracker Filetable destoryed.\n", tracker_trace);

	pthread_mutex_destroy(tracker_file_table_mutex);
	pthread_mutex_destroy(tracker_peer_table_mutex);
	free(tracker_file_table_mutex);
	free(tracker_peer_table_mutex);
	
	shutdown(tracker_socket, 2);
	close(tracker_socket);
	printf("%s trakcer_stop(): Tracker stop successed.\n", tracker_trace);
	
	exit(0);
	
	
}


void tracker_main() {
	
	//1.1 Initialize the peerTable of the tracker side.
	trackerpeerTable_Initialize();
	//1.2 Initialize the trackerFile table for the tracker side.
	fileTable_Initialize();
	
	//1.3 Initialize the tracker.
	
	if((tracker_socket = tracker_init(TRACKER_PORT) )<0){
		printf("%s cannot create tracker socket.\n", tracker_trace);
		exit(1);
	}

	//1.4 Initialize the mutex
	tracker_file_table_mutex	= (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	tracker_peer_table_mutex	= (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(tracker_file_table_mutex, NULL);
	pthread_mutex_init(tracker_peer_table_mutex, NULL);



	//2. register a signal handler which is used to terminate the process
	
	signal(SIGINT,tracker_stop);
	
	//3.1: Create a thread to monitor all peers whether they are alive.
	pthread_t monitor_alive_thread;
	pthread_create(&monitor_alive_thread,NULL,monitor_alive,(void*)0);
	
	//4. Always recived the pkts sent from peer.
	while(1){
		
		int peer_socket;
		struct sockaddr_in peer;
		int peerlen = sizeof(peer);
		ptp_peer_t firstpkt;
		printf("%s Waiting for incoming connections...\n", tracker_trace);
		peer_socket = accept(tracker_socket,(struct sockaddr*)&peer,(socklen_t*)&peerlen);
		
		if (peer_socket > 0) {
			
			// 4.1: First pkt will be the REGISTER pkt from peer.
			recvpkt(peer_socket, &firstpkt);
			
			if (firstpkt.type == REGISTER) {
				
				time_t currTimestamp;
				time(&currTimestamp);
				printf("%s received a REGISTER packet from %s\n", tracker_trace, inet_ntoa(peer.sin_addr));
				//4.2 add this peer to tracker's global peertable.
				tracker_peer_t *tmp = trackerpeerTable_Add(firstpkt.peer_ip, peer_socket, currTimestamp);
				
				//4.3 Send the ptp_tracker_t ack pkt to peer which will set the interval and piece_len of that peer.
				//The ack pkt need has 3 fileds, 1 interval and 2 piece_len are constants, so we only send the 3 file_table to the peer.
				//Using T2P_pkt_init() to initilize this ack pkt.
				
				//4.3.1: Initilize the ack pkt, set interval, piece_len, size, and tracFileTable
				//				ptp_tracker_t* ackpkt;
				//				T2P_pkt_init(ackpkt,
				//							 ALIVE_INTERVAL,PIECE_LENGTH,tfsize,
				//							 tracFileTable);
				
				// T2P_send(ackpkt,peer_socket);
				
				fileTable_Send(peer_socket);
				
				
				// 5.1: using peer_socket as an parameter for handshake communicating with the peer.
				pthread_t handshake_thread;
				pthread_create(&handshake_thread, NULL, handshake, (void *) tmp);
			}
			
		}
		else {
			perror("accept");
			printf("%s track_main(): Cannot accept in while loop.\n", tracker_trace);
		}
	}
	return;
}

