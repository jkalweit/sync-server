#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/epoll.h>


#include "webserver.h"
#include "utils.h"
#include "websockets.h"



static int 
send404(int sfd) {
	char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
	write_string(sfd, response);
}

static int 
send_file(int sfd, const char *filename) {

	int fd = open(filename, O_RDONLY);
	struct stat stat_buf; 
	fstat(fd, &stat_buf);

	write_string(sfd, "HTTP/1.1 200 OK\r\n");
	write_string(sfd, "Content-Type: text/html\r\n");
	write_string(sfd, "Content-Length: ");

	char length[16];
	sprintf(length, "%lu\r\n", stat_buf.st_size);	
	write_string(sfd, length);

	write_string(sfd, "\r\n");

	int n = sendfile(sfd, fd, NULL, stat_buf.st_size);
	printf("					CLOSING [%d]\n", sfd);
	close(fd);
	return 0;
}


static int 
make_socket_non_blocking (int sfd) {
	int flags, s;

	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1)
	{
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
	if (s == -1)
	{
		perror ("fcntl");
		return -1;
	}

	return 0;
}

static int 
create_and_bind (const char *port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE;     /* All interfaces */

	s = getaddrinfo (NULL, port, &hints, &result);
	if (s != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;
		int true=1;
		setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)); // to eliminate "Address in use" TIME_WAIT problem

		s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		if (s == 0)
		{
			/* We managed to bind successfully! */
			break;
		}

		printf("					FAILED [%d]\n", sfd);
		close (sfd);
	}

	if (rp == NULL)
	{
		fprintf (stderr, "Could not bind\n");
		return -1;
	}

	freeaddrinfo (result);

	return sfd;
}




static int 
parse_request(char *request, int sfd) {


	char *verb = strtok(request, " :\r\n");

	printf("verb [%d]: %s\n", sfd, verb);

	if(strcmp(verb, "GET") == 0) {
		//printf("	This is a GET\n");
	} else {
		//printf("	Not a GET\n");
	}

	char *header_name;
	char *header_value;
	int is_websocket_upgrade = 0;
	char *websocket_key;

	char *route = strtok(NULL, " :\r\n");
	//printf("Route: [%s]\n", route);
	if(strcmp(route, "/favicon.ico") == 0 ) {
		send404(sfd);
		printf("					CLOSING [%d]\n", sfd);
		close(sfd);
		return 0;
	} else {
		int lineNum = 0;
		char *line = strtok(NULL, " :\r\n");
		while(line != NULL) {	
			header_name = strtok(NULL, ": \r\n");
			header_value = strtok(NULL, "\r\n");
			if(header_name != NULL && header_value != NULL) {
				header_value = skip_chars(header_value, " ");
				//printf("[%s]: [%s]\n", header_name, header_value);
				if(strcmp(header_name, "Upgrade: websocket") == 0) {
					printf("####### Upgrade websockets!");
					is_websocket_upgrade = 1;
				}	
				if(strcmp(header_name, "Sec-WebSocket-Key") == 0) {
					is_websocket_upgrade = 1;
					websocket_key = header_value;
				}
			} else {
				line = NULL;
			}
		}	
	}

	if(is_websocket_upgrade) {
		websockets_accept(sfd, websocket_key);
	} else {

		if(strcmp(route, "/") == 0) {
			//printf("Sending index.html\n");
			send_file(sfd, "index.html");
		} else {
			send404(sfd);
		}

		printf("					CLOSING [%d]\n", sfd);
		close(sfd);
	}
}


int 
sync_start_server(char *port) {

	int sfd, s, efd;
	struct epoll_event event;
	struct epoll_event *events;


    websockets_init();

	sfd = create_and_bind (port);
	if (sfd == -1)
		abort ();

	s = make_socket_non_blocking (sfd);
	if (s == -1)
		abort ();

	s = listen (sfd, SOMAXCONN);
	if (s == -1)
	{
		perror ("listen");
		abort ();
	}

	efd = epoll_create1 (0);
	if (efd == -1)
	{
		perror ("epoll_create");
		abort ();
	}

	event.data.fd = sfd;
	event.events = EPOLLRDHUP | EPOLLIN | EPOLLET;
	s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
	if (s == -1)
	{
		perror ("epoll_ctl");
		abort ();
	}

	/* Buffer where events are returned */
	events = calloc (MAXEVENTS, sizeof event);

	/* The event loop */
	while (1)
	{
		printf("Listening...\n");
		int n = epoll_wait (efd, events, MAXEVENTS, -1);
		for (int i = 0; i < n; i++)
		{
			printf("fd=[%d] n=[%d]\n", events[i].data.fd, n);

			if(events[i].events & EPOLLRDHUP) {
				printf("!!!!!!!!!!!!HEEEERE [%d]\n", events[i].data.fd);	
				close (events[i].data.fd);
				continue;	
			}
			else if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN)))
			{
				/* An error has occured on this fd, or the socket is not
				   ready for reading (why were we notified then?) */
				fprintf (stderr, "epoll error\n");


				if(websockets_is_websocket(events[i].data.fd)) websockets_remove(events[i].data.fd);
				printf("					CLOSING [%d]\n", events[i].data.fd);
				close (events[i].data.fd);
				continue;
			}

			else if (sfd == events[i].data.fd)
			{
				/* We have a notification on the listening socket, which
				   means one or more incoming connections. */
				while (1)
				{
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

					in_len = sizeof in_addr;
					infd = accept (sfd, &in_addr, &in_len);
					if (infd == -1)
					{
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
						{
							/* We have processed all incoming
							   connections. */
							break;
						}
						else
						{
							perror ("accept");
							break;
						}
					}

					s = getnameinfo (&in_addr, in_len,
							hbuf, sizeof hbuf,
							sbuf, sizeof sbuf,
							NI_NUMERICHOST | NI_NUMERICSERV);
					if (s == 0)
					{
						//printf("Accepted connection on descriptor %d (host=%s, port=%s)\n", infd, hbuf, sbuf);
					}

					/* Make the incoming socket non-blocking and add it to the
					   list of fds to monitor. */
					s = make_socket_non_blocking (infd);
					if (s == -1)
						abort ();

					event.data.fd = infd;
					printf("#####New connection fd [%d]\n", event.data.fd);
					event.events = EPOLLIN | EPOLLET;
					s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
					if (s == -1)
					{
						perror ("epoll_ctl");
						abort ();
					}
				}
				continue;
			}
			else
			{
				/* We have data on the fd waiting to be read. Read and
				   display it. We must read whatever data is available
				   completely, as we are running in edge-triggered mode
				   and won't get a notification again for the same
				   data. */
				int done = 0;

				while (1)
				{
					ssize_t count;
					char buf[512];
					bzero(buf, 512);

					printf("Reading from [%d]\n", events[i].data.fd);
					count = read(events[i].data.fd, buf, sizeof buf);
					printf("	Read [%ld]\n", count);
					printf("	Read from [%d]\n", events[i].data.fd);
					if (count == -1)
					{
						/* If errno == EAGAIN, that means we have read all
						   data. So go back to the main loop. */
						if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
						{
							printf("ERROR [%d]\n", events[i].data.fd);
							perror ("read");
							done = 1;
						}
						break;
					}
					else if (count == 0)
					{
						/* End of file. The remote has closed the
						   connection. */
						done = 1;
						break;
					}

					/* Write the buffer to standard output */
					//s = write (1, buf, count);
					//if (s == -1)
					//{
					//	perror ("write");
					//	abort ();
					//}

					if(websockets_is_websocket(events[i].data.fd)) {
						printf("is_websocket: [%d]\n", websockets_is_websocket(events[i].data.fd));
						websockets_parse_request(buf, count, events[i].data.fd);
					} else {
						parse_request(buf, events[i].data.fd);
					}
				}

				if (done)
				{
					printf ("Closing connection [%d]\n", events[i].data.fd);

					/* Closing the descriptor will make epoll remove it
					   from the set of descriptors which are monitored. */
					if(websockets_is_websocket(events[i].data.fd) == 1) websockets_remove(events[i].data.fd);
					printf("					CLOSING [%d]\n", events[i].data.fd);
					close (events[i].data.fd);
				}
			}
		}
	}

	free (events);


	printf("					CLOSING [%d]\n", sfd);
	close (sfd);

}


