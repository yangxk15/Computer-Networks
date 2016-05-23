/*
 * FILE: file_browser.c 
 *
 * Description: A simple, iterative HTTP/1.0 Web server that uses the
 * GET method to serve static and dynamic content.
 *
 * Date: April 4, 2016
 */

#include <arpa/inet.h>		  // inet_ntoa
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>

#define LISTENQ 9999	// second argument to listen()
#define MAXLINE 4096	// max length of a line
#define RIO_BUFSIZE 8192

typedef struct {
	int rio_fd;				 // descriptor for this buf
	int rio_cnt;				// unread byte in this buf
	char *rio_bufptr;		   // next unread byte in this buf
	char rio_buf[RIO_BUFSIZE];  // internal buffer
} rio_t;

// simplifies calls to bind(), connect(), and accept()
typedef struct sockaddr SA;

typedef struct {
	char filename[512];
	int browser_index;
	off_t offset;			  // for support Range
	size_t end;
} http_request;

//browser name and number of kinds
const char *browser_name[] 	= {"Chrome", "Firefox", "Safari"};
const size_t browser_number = sizeof(browser_name) / sizeof(char *);

typedef struct {
	const char *extension;
	const char *mime_type;
} mime_map;

mime_map meme_types [] = {
	{".css", "text/css"},
	{".gif", "image/gif"},
	{".htm", "text/html"},
	{".html", "text/html"},
	{".jpeg", "image/jpeg"},
	{".jpg", "image/jpeg"},
	{".ico", "image/x-icon"},
	{".js", "application/javascript"},
	{".pdf", "application/pdf"},
	{".mp4", "video/mp4"},
	{".png", "image/png"},
	{".svg", "image/svg+xml"},
	{".xml", "text/xml"},
	{NULL, NULL},
};

char *default_mime_type = "text/plain";

// utility function to get the MIME (Multipurpose Internet Mail Extensions) type
static const char* get_mime_type(const char *filename){
	char *dot = strrchr(filename, '.');
	if(dot){ // strrchar Locate last occurrence of character in string
		mime_map *map = meme_types;
		while(map->extension){
			if(strcmp(map->extension, dot) == 0){
				return map->mime_type;
			}
			map++;
		}
	}
	return default_mime_type;
}

// set up an empty read buffer and associates an open file descriptor with that buffer
void rio_readinitb(rio_t *rp, int fd){
	rp->rio_fd = fd;
	rp->rio_cnt = 0;
	rp->rio_bufptr = rp->rio_buf;
}

/*
 *	This is a wrapper for the Unix read() function that
 *	transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *	buffer, where n is the number of bytes requested by the user and
 *	rio_cnt is the number of unread bytes in the internal buffer. On
 *	entry, rio_read() refills the internal buffer via a call to
 *	read() if the internal buffer is empty.
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n){
	int cnt;
	while (rp->rio_cnt <= 0){  // refill if buf is empty
		rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
		if (rp->rio_cnt < 0){
			if (errno != EINTR) // interrupted by sig handler return
				return -1;
		}
		else if (rp->rio_cnt == 0)  // EOF
			return 0;
		else
			rp->rio_bufptr = rp->rio_buf; // reset buffer ptr
	}
	
	// copy min(n, rp->rio_cnt) bytes from internal buf to user buf
	cnt = n;
	if (rp->rio_cnt < n)
		cnt = rp->rio_cnt;
	memcpy(usrbuf, rp->rio_bufptr, cnt);
	rp->rio_bufptr += cnt;
	rp->rio_cnt -= cnt;
	return cnt;
}

// robustly read a text line (buffered)
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen){
	int n, rc;
	char c, *bufp = usrbuf;
	
	for (n = 1; n < maxlen; n++){
		if ((rc = rio_read(rp, &c, 1)) == 1){
			*bufp++ = c;
			if (c == '\n')
				break;
		} else if (rc == 0){
			if (n == 1)
				return 0; // EOF, no data read
			else
				break;	// EOF, some data was read
		} else
			return -1;	// error
	}
	*bufp = 0;
	return n;
}

// utility function for writing user buffer into a file descriptor
ssize_t rio_written(int fd, void *usrbuf, size_t n){
	size_t nleft = n;
	ssize_t nwritten;
	char *bufp = usrbuf;

	while (nleft > 0){
		if ((nwritten = write(fd, bufp, nleft)) <= 0){
			if (errno == EINTR)  // interrupted by sig handler return
				nwritten = 0;	// and call write() again
			else
				return -1;	   // errorno set by write()
		}
		nleft -= nwritten;
		bufp += nwritten;
	}
	return n;
}

// parse request to get url
int parse_request(int fd, http_request *req){

	// Rio (Robust I/O) Buffered Input Functions
	rio_t rio;
	rio_readinitb(&rio, fd);

	req->browser_index = -1;

	// read all
	char usrbuf[MAXLINE];
	if (rio_readlineb(&rio, usrbuf, sizeof(usrbuf)) <= 0) {
		//no data in the socket or read error occurs
		printf("First line is invalid!\n");
		return -1;
	}

	char *token = strtok(usrbuf, " ");
	if (0 != strncmp(token, "GET", strlen("GET"))) {
		//request method is not get
		printf("The method is not GET!\n");
		return -1;
	}

	token = strtok(NULL, " ");
	if (0 == strncmp(token, "/favicon.ico", strlen("/favicon.ico"))) {
		//fake socket sent by Chrome
		printf("Additional socket!\n");
		return -1;
	}

	// decode url
	char cwd[MAXLINE] = {0};
	getcwd(cwd, MAXLINE);
	strcpy(req->filename, strcat(cwd, token));

	//detect user agent to find out the type of browser
	while (-1 == req->browser_index && rio_readlineb(&rio, usrbuf, sizeof(usrbuf)) > 0) {
		if (0 == strncmp(usrbuf, "User-Agent", strlen("User-Agent"))) {
			token = strtok(usrbuf, " ");
			while ((token = strtok(NULL, " ")) != NULL) {
				if (-1 == req->browser_index) {
					int i;
					for (i = 0; i < browser_number; i++) {
						if (0 == strncmp(token, browser_name[i], strlen(browser_name[i]))) {
							// update recent browser data
							req->browser_index = i;
						}
					}
				}
			}
		}
	}
	
	if (-1 == req->browser_index) {
		//no browser type has been found
		printf("Browser type not identified!\n");
		return -1;
	}

	return 0;
}

// utility function to get the format size
void format_size(char* buf, struct stat *stat){
	if(S_ISDIR(stat->st_mode)){
		sprintf(buf, "%s", "[DIR]");
	} else {
		off_t size = stat->st_size;
		if(size < 1024){
			sprintf(buf, "%lld", size);
		} else if (size < 1024 * 1024){
			sprintf(buf, "%.1fK", (double)size / 1024);
		} else if (size < 1024 * 1024 * 1024){
			sprintf(buf, "%.1fM", (double)size / 1024 / 1024);
		} else {
			sprintf(buf, "%.1fG", (double)size / 1024 / 1024 / 1024);
		}
	}
}

// utility function to format html response
void format_http_response(char *buf, const char *header, const char *data, int status, const char *extension) {
	sprintf(buf,	"HTTP/1.1 %d %s\r\n"	// copy header to buf
					"Content-Length: %lu\r\n"	// copy content length to buf
					"Content-Type: %s\r\n"		// copy content type to buf
					"\r\n"						// CRLF
					"%s",						// copy data
					status, header,
					strlen(data),
					get_mime_type(extension),
					data
	);
}

// serve static content
void serve_static(int out_fd, int in_fd, http_request *req, int status){
	printf("serve_static!\n");

	//data copy	
	char data[MAXLINE] = {0};
	read(in_fd, data, MAXLINE);

	//format http response
	char buf[MAXLINE];
	format_http_response(buf, "OK", data, status, req->filename);
	rio_written(out_fd, buf, MAXLINE);

}

//update recent browsers type and ip
void update(char browser[][100], char ip[][100]) {
	char p1[MAXLINE] = {0};
	char p2[MAXLINE] = {0};
	char buf1[MAXLINE] = {0};
	char buf2[MAXLINE] = {0};
	getcwd(p1, MAXLINE);
	getcwd(p2, MAXLINE);
	strcat(p1, "/recent_browser.txt");
	strcat(p2, "/ip_address.txt");
	int fd1 = open(p1, O_RDONLY | O_CREAT, S_IRUSR |S_IWUSR |S_IXUSR);
	//exclusive lock, any other process will block till they get the lock
	flock(fd1, LOCK_EX);
	int fd2 = open(p2, O_RDONLY | O_CREAT, S_IRUSR |S_IWUSR |S_IXUSR);

	//read file data into memory
	read(fd1, buf1, MAXLINE);
	read(fd2, buf2, MAXLINE);
	int count = 1;
	int i;
	int k;
	for (i = 0, k = 0; i < strlen(buf1); i++) {
		browser[count][k++] = buf1[i];
		if (buf1[i] == '\n') {
			count++;
			if (count == 10 && i < strlen(buf1) - 1) {
				buf1[i + 1] = '\0';
				break;
			}
			k = 0;
		}
	}
	count = 1;
	for (i = 0, k = 0; i < strlen(buf2); i++) {
		ip[count][k++] = buf2[i];
		if (buf2[i] == '\n') {
			count++;
			if (count == 10 && i < strlen(buf2) - 1) {
				buf2[i + 1] = '\0';
				break;
			}
			k = 0;
		}
	}
	close(fd1);
	close(fd2);

	fd1 = open(p1, O_WRONLY | O_TRUNC);
	fd2 = open(p2, O_WRONLY | O_TRUNC);

	//write newest data into files
	write(fd1, browser[0], strlen(browser[0]));
	write(fd1, "\r\n", strlen("\r\n"));
	write(fd1, buf1, strlen(buf1));

	write(fd2, ip[0], strlen(ip[0]));
	write(fd2, "\r\n", strlen("\r\n"));
	write(fd2, buf2, strlen(buf2));

	//release the lock
	flock(fd1, LOCK_UN);
	close(fd1);
	close(fd2);
}

// pre-process files in the "home" directory and send the list to the client
void handle_directory_request(int out_fd, int status, char browser[][100], char ip[][100]){
	printf("handle_directory_request!\n");
	
	char buf[MAXLINE];
	char data[MAXLINE] = {0};
	strcat(data, "<html><head><style>body{font-family: monospace; font-size: 13px;}td {padding: 1.5px 6px;}</style></head><body>");

	strcat(data, "<table>");
	//read all the files in the current working directory
	DIR *dir;
	struct dirent *ent;
	char cwd[MAXLINE] = {0};
	// get file directory
	getcwd(cwd, MAXLINE);
	// read directory
	if ((dir = opendir (cwd)) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;
			int ffd = open(ent->d_name, O_RDONLY, 0);
			struct stat sbuf;
			fstat(ffd, &sbuf);
			char temp[MAXLINE];
			sprintf (temp, "<tr><td><a href=\"%s\">%s</a></td><td>%s</td><td>%lld</td></tr>", ent->d_name, ent->d_name, ctime(&sbuf.st_mtime), sbuf.st_size);
			strcat(data, temp);
			close(ffd);
		}
		closedir (dir);
	}
	else {
		perror ("");
		return;
	}
	strcat(data, "</table>");

	//list recent browsers
	strcat(data, "<table>");
	strcat(data, "<tr><td>The last 10 visited browsers:</td>");
	int i;
	for (i = 0; i < 10; i++) {
		char temp[100];
		sprintf(temp, "<td>%s</td>", browser[i]);
		strcat(data, temp);
	}
	strcat(data, "</tr>");

	//list recent ip
	strcat(data, "<tr><td>The corresponding IP address:</td>");
	for (i = 0; i < 10; i++) {
		char temp[100];
		sprintf(temp, "<td>%s</td>", ip[i]);
		strcat(data, temp);
	}
	strcat(data, "</tr>");
	strcat(data, "</table>");

	strcat(data, "</body>");
	strcat(data, "</html>");

	format_http_response(buf, "OK", data, status, ".html");

	// send response headers to client e.g., "HTTP/1.1 200 OK\r\n"
	// send the file buffers to the client
	// send recent browser data to the client
	rio_written(out_fd, buf, MAXLINE);
}

// echo client error e.g. 404
void client_error(int fd, int status, char *msg, char *longmsg){
	printf("client_error!\n");
	char buf[MAXLINE];
	format_http_response(buf, msg, longmsg, status, "");
	rio_written(fd, buf, MAXLINE);
}

// log files
void log_access(int status, struct sockaddr_in *c_addr, http_request *req){
	printf("*******************************************************************************************\n");
	printf("Access Log: Status = %d, Browser: %s, IP: %s, Requested File/Directory: %s\n",
			status, browser_name[req->browser_index], inet_ntoa(c_addr->sin_addr), req->filename);
	printf("*******************************************************************************************\n\n");
}

// handle one HTTP request/response transaction
void process(int fd, struct sockaddr_in *clientaddr){
	printf("Accept Request, fd is %d, pid is %d\n", fd, getpid());

	http_request req;
	memset(&req, 0, sizeof(req));
	if (-1 == parse_request(fd, &req)) {
		//bad socket, don't record
		printf("Invalid socket!\n");
		return;
	}
	
	//update recent visited client information
	char browser[10][100];
	char ip[10][100];
	memset(browser, 0, 1000);
	memset(ip, 0, 1000);
	strcpy(browser[0], browser_name[req.browser_index]);
	strcpy(ip[0], inet_ntoa(clientaddr->sin_addr));
	update(browser, ip);
	

	struct stat sbuf;
	int status = 200; //server status init as 200
	int ffd = open(req.filename, O_RDONLY, 0);
	if(ffd <= 0){
		// detect 404 error and print error log
		status = 404;
		client_error(fd, status, "Not Found", "File Not Found");
		
	} else {
		// get descriptor status
		fstat(ffd, &sbuf);
		if(S_ISREG(sbuf.st_mode)){
			// server serves static content
			serve_static(fd, ffd, &req, status);
		} else if(S_ISDIR(sbuf.st_mode)){
			// server handle directory request
			handle_directory_request(fd, status, browser, ip);
		} else {
			// detect 400 error and print error log
			status = 400;
			client_error(fd, status, "Bad Request", "Bad Request");
		}
		close(ffd);
	}
	
	// print log/status on the terminal
	log_access(status, clientaddr, &req);
}

// open a listening socket descriptor using the specified port number.
int open_listenfd(int port){

	// create a socket descriptor
	struct sockaddr_in server;
	// 6 is TCP's protocol number
	// enable this, much faster : 4000 req/s -> 17000 req/s
	int sockfd = socket(AF_INET, SOCK_STREAM, 6);

	server.sin_family = AF_INET;
	server.sin_port = htons (port);
	server.sin_addr.s_addr = htonl (INADDR_ANY);

	// eliminate "Address already in use" error from bind.
	int error;
	if ((error = bind (sockfd, (SA *) &server, sizeof (server))) < 0) {
		perror ("bind");
		exit (EXIT_FAILURE);
	}

	// Listenfd will be an endpoint for all requests to port
	// on any IP address for this host
	// make it a listening socket ready to accept connection requests
	listen(sockfd, LISTENQ);
	return sockfd;
}

// main function:
// get the user input for the file directory and port number
int main(int argc, char** argv){
	struct sockaddr_in clientaddr;
	int port = 9999,
		listenfd,
		connfd,
		pid;
	
	// get the name of the current working directory
	// user input checking
	if (argc != 3 || -1 == chdir(argv[1]) || (port = atoi(argv[2])) <= 0) {
		printf("usage:	working_directory(test) port_number(1-9999)\n");
		return 1;
	}

	listenfd = open_listenfd(port);
	// ignore SIGPIPE signal, so if browser cancels the request, it
	// won't kill the whole process.
	signal(SIGPIPE, SIG_IGN);
	// init process collect zombie processes
	signal(SIGCHLD, SIG_IGN);

	while(1){
		// permit an incoming connection attempt on a socket.
		socklen_t clen = sizeof(clientaddr);
		connfd = accept(listenfd, (SA *) &clientaddr, &clen); 

		// fork children to handle parallel clients
		pid = fork();
		if (0 == pid) {
			// handle one HTTP request/response transaction
			process(connfd, &clientaddr);
			close(connfd);
			exit(0);
		}
	}

	return 0;
}
