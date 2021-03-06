#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NO 8080
#define BUF_REQUEST_SIZE 1024

int threadCount = 5;

struct HTTP_request {
	char method[500];
	char path[256];
};

int create_server();
int connect_to_client(int serv_sock);
char * recv_request(int accept_sock);
struct HTTP_request parse_header(char *header, int accept_sock);

FILE * file_exists(char *file_path);

void perform_action(char *path, char *method, int accept_sock);
int open_file(char *path, int accept_sock, int isOpen);
int serve_file(FILE * fpRead, int accept_sock);
void send_header(int result, int accept_sock);
int connect_to_webserver(struct HTTP_request request);
void recv_from_webserver(int web_sock, int local_sock, char *url);

int urlBlacklisted(char *url);
void *client_handler(void *sock_desc);


int main() {
	char buffer[256];
	int serv_sock, accept_sock, num_data_recv, status;
	int *sock_tmp;
	struct sockaddr_in cli_addr;
	socklen_t addr_size;	
	pthread_t a_thread;		

	
	//create socket server
	serv_sock = create_server();

	//accept connection from the client
	//accept_sock = connect_to_client(serv_sock);

	//ADD THREADING
	status = listen(serv_sock, 5);
    if (status < 0) {
        perror("listen() failed");
        close(serv_sock);
        exit(1);
    }

	printf("listening on port %d\n", PORT_NO);

	addr_size = sizeof(cli_addr);

	while(1)
	{
		accept_sock = accept(serv_sock, (struct sockaddr *)&cli_addr, &addr_size);
        if (accept_sock == -1){
            close(serv_sock);
            exit(1);
        }

        sock_tmp = malloc(1);
        *sock_tmp = accept_sock;
        printf("thread count = %d\n", threadCount);
        threadCount--;
        status = pthread_create(&a_thread, NULL, client_handler,
            (void *) sock_tmp);
        if (status != 0) {
            perror("Thread creation failed");
            close(serv_sock);
            close(accept_sock);
            free(sock_tmp);
            exit(1);
        }	
	}



	close(serv_sock);
	close(accept_sock);

	return 0;
}

/******************************************************
 *Create a socket and bind it to a port so that it can 
 * listen for incoming connections.
 * Returns the socket id 
******************************************************/
int create_server() 
{
	int serv_sock, clilen;
	
	struct sockaddr_in serv_addr;
	

	//create socket
	serv_sock = socket(AF_INET, SOCK_STREAM, 0);

	if(serv_sock < 0)
	{
		printf("Socket could not be opened...\n");
		exit(1);
	}

	//initialize socket structure
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT_NO);

	//bind the socket to the port
	if(bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("error while binding...\n");
		exit(1);
	}

	return(serv_sock);
}
/******************************************************
 * Connect to a client with the socket made in create_server().
 * This function listens for a connection from the client
 * The function will accept the connection and return the socket id for this connection
 * Parameters: the socket connected to the client
******************************************************/
// int connect_to_client(int serv_sock)
// {
// 	struct sockaddr_in cli_addr;
// 	socklen_t addr_size;

// 	int accept_sock, num_data_recv;
// 	char buffer[256];

// 	listen(serv_sock, 5);
// 	printf("listening on port %d\n", PORT_NO);

// 	addr_size = sizeof(cli_addr);

// 	accept_sock = accept(serv_sock, (struct sockaddr *)&cli_addr, &addr_size);
// 	if(accept_sock < 0)
// 	{
// 		printf("error accepting\n");
// 		exit(1);
// 	}

// 	return accept_sock;
// }
/******************************************************
 * These tasks were originally in main, but since these
 * tasks are completed at a per thread basis, they needed
 * to be moved to this function
******************************************************/
void *client_handler(void *sock_desc)
{
	char header[1000];
	int web_sock;
	struct HTTP_request client_request;
	int accept_sock = *(int*)sock_desc;
	FILE * fpRead;

	printf("test before checking recv request\n");
	//get the HTTP request from client
	strcpy(header, recv_request(accept_sock));

	//parse the HTTP request and put into struct
	client_request = parse_header(header, accept_sock);	
	
	// Determine what kind of request the client sent
	if(strcasecmp(client_request.method, "GET") == 0)
	{		
		//check if url is blacklisted
		if(urlBlacklisted(client_request.path))
		{
			//send 401
			printf("sending 401...\n");
			send_header(-2, accept_sock);
			pthread_exit("closing this thread");
		}		
		//redirect user to index if they request base url
		if(client_request.path[0] == '\0')
		{
			printf("user requested base url... redirecting to index.html\n");
			strcat(client_request.path, "index.html");
		}
		
		//check to see if there is a cache or a resource file in the server
		fpRead = file_exists(client_request.path);
		
		if(fpRead != NULL)
		{
			//serve the file
			send_header(0, accept_sock); //send 200 OK
			serve_file(fpRead, accept_sock); //send file
		} else {
			//contact webserver
			web_sock = connect_to_webserver(client_request);
			if(web_sock < 0)
			{
				printf("send 404\n");
				send_header(-1, accept_sock);
			} else {
				//get the data from the webserver
				recv_from_webserver(web_sock, accept_sock, client_request.path);
			}				

		}
	//catchall for any other types of requests since they are not required in the assignment
	} else {
		printf("the request: %s is not supported\n", client_request.method);		
	}

	if(fpRead != NULL)
		fclose(fpRead);
	pthread_exit("closing this thread");
}

/****************************************************** 
 * Returns the string request sent by the client
 * Parameters: the socket id for the client connection
 ******************************************************/

char * recv_request(accept_sock)
{
	char static buffer[BUF_REQUEST_SIZE];
	int num_data_recv;

	bzero(buffer, BUF_REQUEST_SIZE);
	printf("before read\n");
	num_data_recv = read(accept_sock, buffer, BUF_REQUEST_SIZE);
	printf("after read\n");
	if(num_data_recv < 0)
	{
		printf("error reading from socket\n");
		exit(1);
	}

	return buffer;
}
/******************************************************
 * Gets the METHOD and PATH from the HTTP request header
 * Returns a stuct that contains the method and path
 ******************************************************/
struct HTTP_request parse_header(char *header, int accept_sock) {	

	struct HTTP_request client_request;

	//counters
	int i = 0;
	int j = 0;
	int k = 0;

	printf("request header:\n%s", header);

	//get the method
	while(!isspace(header[i]))
	{
		client_request.method[j] = header[i];
		i++; j++;
	}	

	do {
		i++;
		
		if(header[i] != '/')
		{
			client_request.path[k] = header[i];			
			k++;
		}
		
	}	while(!isspace(header[i]));
	client_request.path[k-1] = '\0';	

	return client_request;
}
/****************************************************** 
 * Creates a path that the server can understand. 
 * first attempt: add resources/ to the beginning of
 * 		the path to see if the file exists in the resources directory
 * second attempt: append .html to path to look for a cached webpage
 * Returns the file pointer whether it is NULL or assigned.
 ******************************************************/
FILE * file_exists(char *file_path)
{
	FILE *fpRead;
	char resource_path[256];
	printf("in file exists\n");

	//create path to resources directory
	strcpy(resource_path, "resources/");
	strcat(resource_path, file_path);

	fpRead = fopen(resource_path, "r");
	if(fpRead == NULL)
	{			
		//append .html to resource path
		strcat(resource_path, ".html");
		fpRead = fopen(resource_path, "r");
	}

	return fpRead;

}
/****************************************************** 
 * Attempt to connect to a webserver by gettting the domain's
 * IP address and forming a connection. 
 * Returns the socket id if the connection is successful,
 * otherwise, returns a -1
 ******************************************************/
int connect_to_webserver(struct HTTP_request request)
{
	int sockfd, isWritten, isRead;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	printf("looking up %s\n", request.path);
	printf("this may take some time...\n");
	
	server = gethostbyname(request.path);
	

	if(server == NULL)
	{
		herror("can't connect to webserver\n");
		return -1;
	}

	char *ip_address = inet_ntoa(*(struct in_addr *)server->h_addr);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		printf("error creating socket\n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip_address);
	serv_addr.sin_port = htons(80);

	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("error while attempting to connect...\n");
		return -1;
	}

	sprintf(request.method, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", request.path);
	isWritten = write(sockfd, request.method, sizeof(request.method));
	if(isWritten < 0)
	{
		printf("error writing to socket...\n");
		return -1;
	}

	return sockfd;
}
/****************************************************** 
 * Function will recieve data from webserver and cache it 
 * to the resources directory in the format:
 * 		"<webserver address>.html"
 * function will then send the data recieved from the webserver
 * to the client
 ******************************************************/
void recv_from_webserver(int web_sock, int local_sock, char *url)
{
	int bytes_read;
	char buf[1024];
	FILE *fpWrite;
	FILE *fpRead;
	char new_filepath[512];

	//create filename and location for cached website
	strcpy(new_filepath, "resources/");
	strcat(new_filepath, url);
	strcat(new_filepath, ".html");


	fpWrite = fopen(new_filepath, "w+");

	while ((bytes_read = read(web_sock, buf, (sizeof(buf)-1))) > 0)
	{
		buf[bytes_read]='\0';
		//save to file
		fputs(buf, fpWrite);			
		//send to browser/client
		write(local_sock, buf, strlen(buf));
		printf("test local_sock %d\n", local_sock);
		//clear buffer	
		memset(buf, '\0', sizeof(buf));
	}
	fprintf(fpWrite, "%s\n", "THIS IS CHACHED");

	fclose(fpWrite);
	
	return;
}

//this function simply sends a header to the client
void send_header(int result, int accept_sock) {
	int writeSuccess;
	char OK_response[256];
	char BAD_response[256];
	strcpy(OK_response, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=ISO-8859-1\r\n\r\n");
	if (result == 0)
	{
		//send 200
		writeSuccess = write(accept_sock, OK_response, strlen(OK_response));
		printf("test local_sock %d\n", accept_sock);
		printf("***********response***********\n%s", OK_response);
		if(writeSuccess < 0)
		{
			printf("error writing to client\n");
			exit(1);
		}
	} else if (result == -2)
	{
		//send 401	
		strcpy(BAD_response, "HTTP/1.1 401 ACCESS DENIED\r\n");
		strcat(BAD_response, "Content-Type: text/html\r\n\r\n");
		strcat(BAD_response, "<HTML><TITLE>401 Access Denied</TITLE><BODY><H1>Status: 401 Access Denied</H1></BODY></HTML>\r\n\r\n");
		writeSuccess = write(accept_sock, BAD_response, strlen(BAD_response));				
	} else {
		//send 404		
		strcpy(BAD_response, "HTTP/1.1 404 NOT FOUND\r\n");
		strcat(BAD_response, "Content-Type: text/html\r\n\r\n");
		strcat(BAD_response, "<HTML><TITLE>404 Not Found</TITLE><BODY><H1>The server could not fufill your request...</H1></BODY></HTML>\r\n\r\n");
		writeSuccess = write(accept_sock, BAD_response, strlen(BAD_response));		
	}
}

/******************************************************
 * Function sends a file to the client by dynamically 
 * allocating space for a buffer that will temporarily 
 * hold the entire file and push it to the client
 * Returns 0 on success
 * Parameters: the file pointer for the file to be sent
 	the socket that will be recieving the file
******************************************************/
int serve_file(FILE *fpRead, int accept_sock)
{
	char buf[1024];
	int writeSuccess;	
	
	bzero(buf, sizeof(buf));

	do {
		fgets(buf, sizeof(buf), fpRead);		
		writeSuccess = write(accept_sock, buf, strlen(buf));		
		bzero(buf, sizeof(buf));
	} while(!feof(fpRead));
	return 0;
}

int urlBlacklisted(char *url)
{
	FILE *fp;
	char blackListURL[256];
	int i = 0;
	fp = fopen("blacklist.txt","r");

	while(!feof(fp))
	{
		fgets(blackListURL, sizeof(blackListURL), fp);		
		if(strcmp(blackListURL, url) == 0)
		{
			//close file and send success code			
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);
	return 0;
}