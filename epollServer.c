/*---------------------------------------------------------------------------------------
--	SOURCE FILE:	EpollServer.c
--
--	PROGRAM:		epollServer
--
--	FUNCTIONS:		static void SystemFatal (const char* message)
--					static int ClearSocket (int fd)
--					void close_fd (int)
--					void handleData(int*)
--					void setupSignal()
--					void setupListenSocket(int)
--					int handleError(int * i)
--					void setupFD(struct epoll_event * event)
--					int handleConnection(struct epoll_event * event, int * i)
--					void handleData(int * i)
--
--	DATE:			February 15, 2014
--
--	DESIGNERS:		Jacob Miner
--
--	PROGRAMMERS:	Jacob Miner
--
--	NOTES:
--	Single threaded TCP echo server using Epoll to service clients.
---------------------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define TRUE 			1
#define FALSE 			0
#define EPOLL_QUEUE_LEN	256
#define BUFLEN			80
#define SERVER_PORT		7000

typedef struct {
	int epoll_fd;
	struct epoll_event events[EPOLL_QUEUE_LEN];
} epollWrapper;

//Globals
int fd_server;
int websocket;
epollWrapper info;

// Function prototypes
static void SystemFatal (const char* message);
static int ClearSocket (int fd);
void close_fd (int);
void handleData(int*);
void setupSignal();
void setupListenSocket(int);
int handleError(int * i);
void setupFD(struct epoll_event * event);
int handleConnection(struct epoll_event * event, int * i);
void handleData(int * i);

void connectToWebserver()
{
    struct hostent *hp;
	struct sockaddr_in server;
	char  *host;

    if ((websocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Cannot create socket");
		exit(1);
	}
    
    bzero((char *)server, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	server->sin_port = htons(80);
	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server->sin_addr, hp->h_length);
    
	if (connect (*sd, (struct sockaddr *)server, sizeof(*server)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		perror("connect");
		exit(1);
	}

}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    main
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  main(argc, char* argv[])
--
--  RETURNS:  int - 0 on success
--
--  NOTES: The main thread of the program. Calls all other functions.
--  
------------------------------------------------------------------------------*/
int main (int argc, char* argv[]) 
{
	int i; 
	int num_fds;
	static struct epoll_event event;
	int port = SERVER_PORT;


    connectToWebserver();

    setupSignal();
	setupListenSocket(port);
	setupFD(&event);
    
	// Execute the epoll event loop
	while (TRUE) 
	{
		//struct epoll_event events[MAX_EVENTS];
		num_fds = epoll_wait (info.epoll_fd, info.events, EPOLL_QUEUE_LEN, -1);
		if (num_fds < 0) 
			SystemFatal ("Error in epoll_wait!");

		for (i = 0; i < num_fds; i++) 
		{
    		// Case 1: Error condition
			if (handleError(&i))
				continue;

    		// Case 2: Server is receiving a connection request
    		if (handleConnection(&event, &i))
    			continue;
			
			handleData(&i);
		}
	}
	close(fd_server);
	exit (EXIT_SUCCESS);
}


/*------------------------------------------------------------------------------
--
--  FUNCTION:    setupSignal
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  setupSignal()
--
--  RETURNS:  void
--
--  NOTES: Sets up the signal handler to call close_fd when SIGINT is called.
--  
------------------------------------------------------------------------------*/
void setupSignal()
{
	struct sigaction act;
	act.sa_handler = close_fd;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
    {
            perror ("Failed to set SIGINT handler");
            exit (EXIT_FAILURE);
    }	
}


/*------------------------------------------------------------------------------
--
--  FUNCTION:    setupListenSocket
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  setupListenSocket(int port)
--							port - the listening socket to be set up.
--
--  RETURNS:  void
--
--  NOTES: Sets up the listening socket, handles error.s
--  
------------------------------------------------------------------------------*/
void setupListenSocket(int port)
{
	int arg;
	struct sockaddr_in addr;

	fd_server = socket (AF_INET, SOCK_STREAM, 0);
	if (fd_server == -1) 
		SystemFatal("socket");
	
	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
	arg = 1;
	if (setsockopt (fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) 
		SystemFatal("setsockopt");
	
	// Make the server listening socket non-blocking
	if (fcntl (fd_server, F_SETFL, O_NONBLOCK | fcntl (fd_server, F_GETFL, 0)) == -1) 
		SystemFatal("fcntl");
	
	// Bind to the specified listening port
	memset (&addr, 0, sizeof (struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	
	if (bind (fd_server, (struct sockaddr*) &addr, sizeof(addr)) == -1) 
		SystemFatal("bind");
}


/*------------------------------------------------------------------------------
--
--  FUNCTION:    setupFD
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  setupFD(struct epoll_event * event)
--							event - the event structure necessary for epoll
--
--  RETURNS:  void
--
--  NOTES: Wrapper for setting up the file descriptors and event structures for 
--			epoll to function.
--  
------------------------------------------------------------------------------*/
void setupFD(struct epoll_event * event)
{
	// Listen for fd_news; SOMAXCONN is 128 by default
	if (listen (fd_server, SOMAXCONN) == -1) 
		SystemFatal("listen");
	
	// Create the epoll file descriptor
	info.epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
	if (info.epoll_fd == -1) 
		SystemFatal("epoll_create");
	
	// Add the server socket to the epoll event loop
	event->events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
	event->data.fd = fd_server;
	if (epoll_ctl (info.epoll_fd, EPOLL_CTL_ADD, fd_server, event) == -1) 
		SystemFatal("epoll_ctl");
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    handleError
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  handleError(int * i)
--							i - the place in the events array where the error occured.
--
--  RETURNS:  int - returns 1 if there is an EPOLLERR, otherwise 0
--
--  NOTES: Handles errors in the event loop for epoll
--  
------------------------------------------------------------------------------*/
int handleError(int * i)
{
	if (info.events[*i].events & (EPOLLHUP | EPOLLERR)) 
	{
		fputs("epoll: EPOLLERR", stderr);
		close(info.events[*i].data.fd);
		return 1;
	}
	assert (info.events[*i].events & EPOLLIN);
	return 0;
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    handleConnection
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  handleConnection(struct epoll_event * event, int * i)
--							event - the event structure necessary for epoll
--							i - the place in the events array where the error occured.
--
--  RETURNS:  int - 1 if a client connects, zero otherwise.
--
--  NOTES:    Handles connections in the event loop for epoll
--  
------------------------------------------------------------------------------*/
int handleConnection(struct epoll_event * event, int * i)
{
	int fd_new;
	struct sockaddr_in remote_addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);

	if (info.events[*i].data.fd == fd_server) 
	{
		//socklen_t addr_size = sizeof(remote_addr);
		fd_new = accept (fd_server, (struct sockaddr*) &remote_addr, &addr_size);
		if (fd_new == -1) 
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK) 
			{
				perror("accept");
			}
			return 1;
		}

		// Make the fd_new non-blocking
		if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1) 
			SystemFatal("fcntl");
		
		// Add the new socket descriptor to the epoll loop
		event->data.fd = fd_new;
		if (epoll_ctl (info.epoll_fd, EPOLL_CTL_ADD, fd_new, event) == -1) 
			SystemFatal ("epoll_ctl");
		
		return 1;
	}

	return 0;
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    handleData
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  handleData(int * i)
--					i - the place in the events array where the error occured.
--
--  RETURNS:  void
--
--  NOTES: Handles the data in the event loop for epoll
--  
------------------------------------------------------------------------------*/
void handleData(int * i)
{
	if (!ClearSocket(info.events[*i].data.fd)) 
	{
		// epoll will remove the fd from its set
		// automatically when the fd is closed
		close (info.events[*i].data.fd);
	}
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    ClearSocket
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  ClearSocket(int fd)
--							fd - the file descriptor to collect data from and echo to
--
--  RETURNS:  int - returns 0 when completed.
--
--  NOTES: Reads from the client, then echos the data back.
--  
------------------------------------------------------------------------------*/
static int ClearSocket (int fd) 
{
	int	n, bytes_to_read;
	char	*bp, buf[BUFLEN];

	bp = buf;
	bytes_to_read = BUFLEN;

	do
    {
        n = 0;
        while ((n = recv (fd, bp, bytes_to_read, 0)) < BUFLEN)
    	{
            if (n <= 0)
                break;

    		bp += n;
    		bytes_to_read -= n;
    	}

    	if (n > 0)
        {
            send (fd, buf, BUFLEN, 0);
        }
    } while (n > 0);
    return FALSE;
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    SystemFatal
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  SystemFatal(const char* message)
--							message - addition message to print with the error.
--
--  RETURNS:  void
--
--  NOTES: Prints the error stored in errno and aborts the program.
--  
------------------------------------------------------------------------------*/
static void SystemFatal(const char* message) 
{
    perror (message);
    exit (EXIT_FAILURE);
}


/*------------------------------------------------------------------------------
--
--  FUNCTION:    close_fd
--
--  DATE:       February 15, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  close_fd(int signo)
--							signo - necessary argument for signal handling functopn/
--
--  RETURNS:  void
--
--  NOTES: The sigint handler. Closes fd_server to prevent memory leaks, then exits porgram
--  
------------------------------------------------------------------------------*/
void close_fd (int signo)
{
    close(fd_server);
	exit (EXIT_SUCCESS);
}
