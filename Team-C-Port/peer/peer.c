#include "peer.h"
#include <dirent.h>

char *myIP;
int tracker_conn;
extern filenode *file_table;
extern peer_peer_t *peer_peer_table;
extern int peer_peer_table_size;

pthread_mutex_t *peer_file_table_mutex;
pthread_mutex_t *peer_peer_table_mutex;


const char *peer_trace	= "Peer:";

int connectToTracker()
{
	struct sockaddr_in server_addr;

	struct hostent *hostInfo;
	hostInfo = gethostbyname(TRACKER_IP);
	server_addr.sin_family = hostInfo->h_addrtype;
	memcpy((char *) &server_addr.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
	server_addr.sin_port = htons(TRACKER_PORT);

	int conn; 
	if ((conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("%s cannot create socket!\n", peer_trace);
		return -1;
	}
	if (connect(conn, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)) < 0) {
		printf("%s cannot connect to the tracker!\n", peer_trace);
		return -1;
	}
	return conn;
}

//upload data to the remote peer
void *P2PUpload(void *arg)
{
	int conn = *(int *) arg;

	char filename[FILE_NAME_LEN];
	int start;
	int end;

	// recv the required name and range
	while (recvfunc(conn, filename, sizeof(filename)) > 0) {
		if (recvfunc(conn, &start, sizeof(start)) < 0) {
			printf("%s p2p upload failed @start!\n", peer_trace);
			return NULL;
		}
		if (recvfunc(conn, &end, sizeof(end)) < 0) {
			printf("%s p2p upload failed @end!\n", peer_trace);
			return NULL;
		}

		FILE *f;
		f = fopen(filename, "r");
		if (f == NULL) {
			printf("%s p2p upload file doesn't exist!\n", peer_trace);
			return NULL;
		}
		fseek(f, start, SEEK_SET);
		char buf[MAX_DATA_LEN];
	
		// divide and send
		while (start != end) {
			int l = MIN(end - start, MAX_DATA_LEN);
			fread(buf, l, 1, f);
			int n = sendfunc(conn, &buf, l);
			if (n < 0) {
				printf("%s p2p upload failed!\n", peer_trace);
				return NULL;
			}
			//printf("%s p2p upload %d bytes!\n", peer_trace, n);
			start += l;
		}
		fclose(f);
	}
	close(conn);
	
	return NULL;
}

//listen on the P2P port; when receiving a data request from another peer, create a P2PUpload Thread
void *P2PListen(void *arg)
{
	int serv_sd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family			= AF_INET;
	server_addr.sin_addr.s_addr		= htonl(INADDR_ANY);
	server_addr.sin_port			= htons(PEER_PORT);
	
	bind(serv_sd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	listen(serv_sd, 1);

	while (1) {
		struct sockaddr_in peer_addr;
		socklen_t peer_addr_len = sizeof(peer_addr);
		int *conn = (int *) malloc(sizeof(int));
		*conn = accept(serv_sd, (struct sockaddr *) &peer_addr, &peer_addr_len);
		if (*conn < 0) {
			free(conn);
			perror("P2PListen: accept");
			return NULL;
		}
		//analyse the data request from another peer
		//create a P2PUpload thread if needed
		pthread_t p2p_upload_thread;
		pthread_create(&p2p_upload_thread, NULL, P2PUpload, (void *) conn);
	}
}

// download data from the remote peer
void *P2PDownload(void *arg)
{
	peer_peer_t *p = (peer_peer_t *) arg;

	// send the required name and range
	if (sendfunc(p->sockfd, p->filename, sizeof(p->filename)) < 0) {
		printf("%s p2p download failed @filename!\n", peer_trace);
		pthread_exit((void *) p);
	}
	if (sendfunc(p->sockfd, &p->start, sizeof(p->start)) < 0) {
		printf("%s p2p download failed @start!\n", peer_trace);
		pthread_exit((void *) p);
	}
	if (sendfunc(p->sockfd, &p->end, sizeof(p->end)) < 0) {
		printf("%s p2p download failed @end!\n", peer_trace);
		pthread_exit((void *) p);
	}
	int s = 0;
	int e = p->end - p->start;

	// read file from peer
	//FILE *f;
	//f = fopen(p->filename, "r+");
	//assert(f != NULL);
	//fseek(f, p->start, SEEK_SET);

	//char buf[MAX_DATA_LEN];

	// recv and gather
	while (s != e) {
		int l = MIN(e - s, MAX_DATA_LEN);
		int n = recvfunc(p->sockfd, p->addr + s, l);
		if (n < 0) {
			printf("%s p2p download from %s failed!\n", peer_trace, p->IP);
			pthread_exit((void *) p);
		}
		//printf("%s p2p download %d bytes from %s!\n", peer_trace, n, p->IP);
		s += l;
	}

	//fclose(f);
	printf("%s Downloading from %s range (%d - %d) finished!\n", peer_trace, p->IP, p->start, p->end);
	pthread_exit(NULL);
}

// partition the file and download from different peers
void Download(char *name, int size, filenode *global)
{
	// get all the available peers to download into the peertable
	int count = 0;
	filenode *p = global;
	while ((p = fileTable_Exist(p, name, NULL)) != NULL) {
		pthread_mutex_lock(peer_peer_table_mutex);
		peerpeerTable_Add(p);
		pthread_mutex_unlock(peer_peer_table_mutex);
		count++;
		p = p->next;
	}
	//printf("%s Before downloading file %s:\n", peer_trace, name);
	//peerpeerTable_Print();

	FILE *f;
	f = fopen(name, "w");

	int c;
	while (1) {
		c = 0;
		fseek(f, 0, SEEK_SET);
		char buf[PIECE_LENGTH];
		int start = 0;
		while (c == 0 && start < size) {
			int end = MIN(size, start + PIECE_LENGTH);
			int seg_len = (end - start) / count;
			int s = start;
			int i = 0;
			pthread_t *thd = (pthread_t *) malloc(sizeof(pthread_t) * count);
			peer_peer_t *peer_t = peer_peer_table;
			pthread_mutex_lock(peer_peer_table_mutex);
			while ((peer_t = peerpeerTable_Exist(peer_t, name)) != NULL) {
				peer_t->addr = buf;
				peer_t->start = s;
				peer_t->end = MIN(end, s + seg_len);
				pthread_create(&thd[i++], NULL, P2PDownload, (void *) peer_t);
				s = peer_t->end;
				peer_t = peer_t->next;
			}	
			//printf("%s During downloading file %s:\n", peer_trace, name);
			//peerpeerTable_Print();
			pthread_mutex_unlock(peer_peer_table_mutex);
			// wait for all the threads to terminate
			int j;
			for (j = 0; j < i; j++) {
				void *r;
				pthread_join(thd[j], &r);
				if (r != NULL) {
					// download thread failed!
					peerpeerTable_Delete((peer_peer_t *) r);
					count--;
					c = 1;
				}
			}
			free(thd);
			fwrite(buf, end - start, 1, f);
			start = end;
		}
		//printf("%s After downloading file %s:\n", peer_trace, name);
		//peerpeerTable_Print();
		if (c == 0) {
			// download success!
			printf("%s Successfully download file %s!\n", peer_trace, name);
			break;
		}
		if (count == 0) {
			// download failed!
			printf("%s All peers have lost the file %s!\n", peer_trace, name);
			break;
		}
		
		printf("%s Some peer is down! Redownloading...\n", peer_trace);
	}
	fclose(f);
	if (count == 0) {
		remove(name);
	}

	peer_peer_t *peer_t = peer_peer_table;
	while ((peer_t = peerpeerTable_Exist(peer_t, name)) != NULL) {
		peer_peer_t *tmp = peer_t;
		peer_t = peer_t->next;
		peerpeerTable_Delete(tmp);
	}
}

int getFileSize(const char *name)
{
	int l;
	FILE *f = fopen(name, "r");
	if (f == NULL) {
		return -1;
	}
	fseek(f, 0, SEEK_END);
	l = ftell(f);
	fclose(f);
	return l;
}

//monitor a local file directory; send out updated file table to the tracker if any file changes in the local file directory
void *FileMonitor(void *arg)
{
	ptp_peer_t pkt;
	p2T_pkt_set(&pkt, 0, NULL, FILE_UPDATE, NULL, myIP, PEER_PORT, 0, NULL);

	//monitor a local file directory
	int fd;
	int wd;
	char buffer[BUF_LEN];
	
	fd = inotify_init();
	
	if (fd < 0) {
		perror("inotify_init");
	}
	
	wd = inotify_add_watch(fd, ".", IN_MODIFY | IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_DELETE_SELF);

	int state = 0;
	
	int out = 0;
	while (out == 0) {
		int i = 0;
		int length = read(fd, buffer, BUF_LEN);  
 		if (length < 0) {
  			perror("read");
		}
  		while (i < length) {
			struct inotify_event *event = (struct inotify_event *) &buffer[ i ];
			if (event->len && event->name[0] != '.') {
				if (event->mask & IN_DELETE_SELF) {
					out = 1;
					break;
				}
				else if (event->mask & IN_DELETE) {
					pthread_mutex_lock(peer_file_table_mutex);
					if (event->mask & IN_ISDIR) {
						printf("The directory %s was deleted.\n", event->name);       
					}
					else {
						printf("The file %s was deleted.\n", event->name);
					}
					
					filenode *p = fileTable_Exist(file_table, event->name, myIP);
					if (p != NULL) {
						// if the file is still in the table, that means the peer itself delete it instead of the tracker told it to
						fileTable_Delete(event->name, myIP);
						// then we need to send the updated table to the tracker
						if (sendpkt(tracker_conn, &pkt) < 0) {
							printf("%s updated local file table sending failed!\n", peer_trace);
							return NULL;
						}
						
					}
					pthread_mutex_unlock(peer_file_table_mutex);
				}
				else if (event->mask & IN_CREATE) {
					state = 2;
					//printf("create %s\n", event->name);
				}
				else if (event->mask & IN_MODIFY) {
					//printf("modify %s\n", event->name);
					if (state == 0) {
						state = 1;
					}
					
				}
				else if (event->mask & IN_CLOSE_WRITE) {
					//printf("close write %s\n", event->name);
					pthread_mutex_lock(peer_file_table_mutex);
					if (state == 2) {
						if (event->mask & IN_ISDIR) {
							printf("The directory %s was created.\n", event->name);       
						}
						else {
							printf("The file %s was created.\n", event->name);
						}

						filenode tmp;
						memset(&tmp, 0, sizeof(filenode));
						strcpy(tmp.filename, event->name);
						tmp.filesize = getFileSize(tmp.filename);
						if (tmp.filesize < 0) {
							state = 0;
							pthread_mutex_unlock(peer_file_table_mutex);
							continue;
						}
						tmp.timestamp = 0;
						strcpy(tmp.peerIP, myIP);

						// try to add this node into the local file table
						// if there's already such a node in the table, that means this file is downloaded
						// otherwise it's created
						
						filenode* p = fileTable_Exist(file_table, event->name, NULL);
						if (p == NULL) {
							// locally created a file
							fileTable_Add(&tmp, myIP);
						}
						else {
							if (strcmp(p->peerIP, myIP) != 0) {
								// newly downloaded
								strcpy(p->peerIP, myIP);
							}
							else {
								// vim version update
								p->filesize = getFileSize(event->name);
								p->timestamp = 0;
							}
						}
						printf("%s Updated local file table:\n", peer_trace);
						fileTable_Print(file_table);

						// either way we should send the updated table to the tracker
						if (sendpkt(tracker_conn, &pkt) < 0) {
							printf("%s updated local file table sending failed!\n", peer_trace);
							return NULL;
							
						}
					}
					else if (state == 1) {
						if (event->mask & IN_ISDIR) {
							printf("The directory %s was modified.\n", event->name);
						}
						else {
							printf("The file %s was modified.\n", event->name);
						}

						filenode *p = fileTable_Exist(file_table, event->name, NULL);
						if (p != NULL) {
							if (strcmp(p->peerIP, myIP) == 0) {
								// the file is modified locally, reset the size and the timestamp and send to the tracker
								p->filesize = getFileSize(event->name);
								p->timestamp = 0;
							}
							else {
								strcpy(p->peerIP, myIP);
							}
							printf("%s Updated local file table:\n", peer_trace);
							fileTable_Print(file_table);
							if (sendpkt(tracker_conn, &pkt) < 0) {
								printf("%s updated local file table sending failed!\n", peer_trace);
								return NULL;
							}
						}
						else {
							printf("I should not be here!\n");
						}
					}
					state = 0;
					pthread_mutex_unlock(peer_file_table_mutex);
				}
			}
			i += EVENT_SIZE + event->len;
		}
	}

	(void) inotify_rm_watch(fd, wd);
	(void) close(fd);
	exit(0);
}

//send out heartbeat (alive) messages to the tracker to keep its online status
void *Alive(void *arg)
{
	ptp_peer_t pkt;
	p2T_pkt_set(&pkt, 0, NULL, KEEP_ALIVE, NULL, myIP, PEER_PORT, 0, NULL);
	while (1) {
		sleep(ALIVE_INTERVAL);
		if (sendpkt(tracker_conn, &pkt) < 0) {
			printf("%s KEEP_ALIVE sending failed!\n", peer_trace);
			return NULL;
		}
	}
}
void peer_stop() {
	static int p = 0;
	if (p > 0) {
		return;
	}
	p++;
	printf("%s Stop!\n", peer_trace);

	close(tracker_conn);

	pthread_mutex_lock(peer_file_table_mutex);
	fileTable_Destroy(file_table);
	pthread_mutex_unlock(peer_file_table_mutex);
	pthread_mutex_lock(peer_peer_table_mutex);
	peerpeerTable_Destroy();
	pthread_mutex_unlock(peer_peer_table_mutex);

	pthread_mutex_destroy(peer_file_table_mutex);
	pthread_mutex_destroy(peer_peer_table_mutex);
	free(peer_file_table_mutex);
	free(peer_peer_table_mutex);
	// there might be more to free
}
void peer_main()
{
	// initialize the tables
	tracker_conn = -1;
	fileTable_Initialize();
	peerpeerTable_Initialize();

	// initialize the mutex
	peer_file_table_mutex	= (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	peer_peer_table_mutex	= (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(peer_file_table_mutex, NULL);
	pthread_mutex_init(peer_peer_table_mutex, NULL);

	//register peer_stop
	signal(SIGINT,peer_stop);

	myIP = my_ip();
	printf("%s Local ip address is %s\n", peer_trace, myIP);

	// try to connect to the tracker
	tracker_conn = connectToTracker();
	if (-1 == tracker_conn) {
		printf("%s Can't connect to tracker", peer_trace);
		return;
	}
	printf("%s Connection to the tracker established.\n", peer_trace);

	ptp_peer_t pkt;
	p2T_pkt_set(&pkt, 0, NULL, REGISTER, NULL, myIP, PEER_PORT, 0, NULL);

	if (sendpkt(tracker_conn, &pkt) < 0) {
		printf("%s REGISTER sending failed!\n", peer_trace);
		peer_stop();
		return;
	}

	// create the alive thread
	pthread_t alive_thread;
	pthread_create(&alive_thread, NULL, Alive, NULL);
	printf("%s Alive thread created.\n", peer_trace);

	// create a P2P Listen thread
	pthread_t p2p_listen_thread;
	pthread_create(&p2p_listen_thread, NULL, P2PListen, NULL);
	printf("%s p2p listen thread created.\n", peer_trace);
	
	// create a file monitor thread
	char dir_name[FILE_NAME_LEN];
	printf("%s Please input the directory which you would like to monitor:", peer_trace);
	scanf("%s", dir_name);

	// create a loval file directory to monitor
	mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	chdir(dir_name);
	
	DIR *dir = opendir(".");
	if (dir == NULL) {
		printf("%s The directory which you input is invalid.\n", peer_trace);
		peer_stop();
		return;
	}
        struct dirent *entry;
	pthread_mutex_lock(peer_file_table_mutex);
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) {
			continue;
		}
		filenode tmp;
		memset(&tmp, 0, sizeof(filenode));
		strcpy(tmp.filename, entry->d_name);
		tmp.filesize = getFileSize(tmp.filename);
		if (tmp.filesize < 0) {
			continue;
		}
		tmp.timestamp = 0;
		strcpy(tmp.peerIP, myIP);
		fileTable_Add(&tmp, myIP);
	}
	pthread_mutex_unlock(peer_file_table_mutex);
	
	pthread_t file_monitor_thread;
	pthread_create(&file_monitor_thread, NULL, FileMonitor, (void *) ".");
	printf("%s file monitor thread created.\n", peer_trace);
	

//	int setup = 0;


	filenode *global_table;
	for (;;) {
		if (fileTable_Receive(tracker_conn, &global_table) < 0) {
			printf("%s Connection to the tracker is lost. Exiting...\n", peer_trace);
			break;
		}
		printf("%s Received a file table from the tracker:\n", peer_trace);
		fileTable_Print(global_table);
		pthread_mutex_lock(peer_file_table_mutex);

		// based on the global file table, update the local file table
		filenode *cur = global_table;
		while (cur != NULL) {
			if (cur != fileTable_Exist(global_table, cur->filename, NULL)) {
				// this file has been processed before
				cur = cur->next;
				continue;
			}

			filenode *tmp = fileTable_Exist(file_table, cur->filename, NULL);

			if (tmp == NULL) {
				// if our local file table doesn't have this file, should we add it?
				if (fileTable_Exist(global_table, cur->filename, myIP) != NULL) {
					// if this ip used to have the file in the global table, that means we just deleted locally, but the tracker hasn't updated it yet
					// thus we should not add this file, cause the delete action is the latest one
					cur = cur->next;
					continue;
				}
				else {
					// if this ip never has this file, we should add it
					fileTable_Add(cur, "");
					Download(cur->filename, cur->filesize, global_table);
				}
			}

			// if out local file table has this file, should we update it?
			else {
				// if this file is newly created but hasn't been marked with a timestamp from the tracker
				if (tmp->timestamp == 0) {
//					filenode *p = fileTable_Exist(global_table, cur->filename, myIP);
//					if (p != NULL && p->timestamp == fileTable_latest(global_table, cur->filename)) {
//						// update the timestamp given from the tracker
						tmp->timestamp = cur->timestamp;
//					}
//					else {
//						// some peer created this new file with the same name first or updated it first
//						// then we shuold change our name
//						strcat(strcat(tmp->name, " from "), myIP);
//					}
				}
				else {
					if (difftime(tmp->timestamp, cur->timestamp) >= 0) {
						// this file is the latest one, do nothing
						cur = cur->next;
						continue;
					}
					else {
						// not the latest one
						// need to update
						//remove(cur->filename);
						//fileTable_Add(cur, myIP);
						tmp->filesize = cur->filesize;
						tmp->timestamp = cur->timestamp;
						memset(tmp->peerIP, 0, IP_LEN);
						Download(cur->filename, cur->filesize, global_table);
					}
				}
			}
			cur = cur->next;
		}

//		if (setup > 0) {
//		//traverse the local file table to see if there's anything need to be deleted
			cur = file_table;
			while (cur != NULL) {
				if (fileTable_Exist(global_table, cur->filename, NULL) == NULL/* && cur->timestamp != 0*/) {
					// the global table doesn't have this file and the timestamp used to be assigned by the tracker
					remove(cur->filename);
					fileTable_Delete(cur->filename, myIP);
					// what if we can't delete this file
				}
				cur = cur->next;
			}
//		}
//		setup++;
		
		printf("%s Updated local file table:\n", peer_trace);
		fileTable_Print(file_table);
		fileTable_Destroy(global_table);
		pthread_mutex_unlock(peer_file_table_mutex);
	}

	peer_stop();
}

