//FILE: server/app_server.c

//Description: this is the server application code. The server first starts the overlay by creating a direct TCP link between the client and the server. Then it initializes the SRT server by calling srt_svr_init(). It creates 2 sockets and waits for connection from the client by calling srt_svr_sock() and srt_svr_connect() twice. Finally the server closes the socket by calling srt_server_close(). Overlay is stoped by calling overlay_end().

//Date: April 18,2008

//Input: None

//Output: SRT server states

#include "srt_server.h"

//two connection are created, one uses client port 87 and server port 88. the other uses client port 89 and server port 9
#define CLIENTPORT1 87
#define SVRPORT1 88
#define CLIENTPORT2 89
#define SVRPORT2 90
//after the connections are created, wait 10 seconds, and then close the connections
#define WAITTIME 10

//this function starts the overlay by creating a direct TCP connection between the client and the server. The TCP socket descriptor is returned. If the TCP connection fails, return -1. The TCP socket descriptor returned will be used by SRT to send segments.
int overlay_start() {
	int tcpserv_sd;
	struct sockaddr_in tcpserv_addr;
	int connection;
	struct sockaddr_in tcpclient_addr;
	socklen_t tcpclient_addr_len;

	tcpserv_sd = socket(AF_INET, SOCK_STREAM, 0); 
	if(tcpserv_sd<0) 
		return -1;
	memset(&tcpserv_addr, 0, sizeof(tcpserv_addr));
	tcpserv_addr.sin_family = AF_INET;
	tcpserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	tcpserv_addr.sin_port = htons(PORT);

	if(bind(tcpserv_sd, (struct sockaddr *)&tcpserv_addr, sizeof(tcpserv_addr))< 0)
		return -1; 
	if(listen(tcpserv_sd, 1) < 0) 
		return -1;
	printf("waiting for connection\n");
	connection = accept(tcpserv_sd,(struct sockaddr*)&tcpclient_addr,&tcpclient_addr_len);
	return connection;
}

//this function stops the overlay by closing the tcp connection between the server and the clien
void overlay_stop(int connection) {
	close(connection);
}

int main() {
	//random seed for segment loss
	srand(time(NULL));

	//start overlay and get the overlay TCP socket descriptor
	int overlay_conn = overlay_start();
	if(overlay_conn<0) {
		printf("can not start overlay\n");
	}

	//initialize srt server
	srt_server_init(overlay_conn);

	/*create a server sock */	
	//create a srt server sock at port SVRPORT1 
	int sockfd= srt_server_sock(SVRPORT1);
	if(sockfd<0) {
		printf("can't create srt server\n");
		exit(1);
	}
	//listen and accept connection from a srt client 
	srt_server_accept(sockfd);

	/*another server sock*/
	//create a srt server sock at port SVRPORT2
	int sockfd2= srt_server_sock(SVRPORT2);
	if(sockfd2<0) {
		printf("can't create srt server\n");
		exit(1);
	}
	//listen and accept connection from a srt client 
	srt_server_accept(sockfd2);


	sleep(WAITTIME);

	//close srt server 
	if(srt_server_close(sockfd)<0) {
		printf("can't destroy srt server\n");
		exit(1);
	}				
	if(srt_server_close(sockfd2)<0) {
		printf("can't destroy srt server\n");
		exit(1);
	}				

	//stop the overlay
	overlay_stop(overlay_conn);
}
