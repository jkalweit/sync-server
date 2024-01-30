#include "webserver.h"

volatile int WEBSERVER_RUNNING;

static struct RouteHandler  ROUTE_HANDLERS[WEBSERVER_MAXROUTES];
static int                  ROUTE_HANDLER_COUNT = 0;


int
webserver_add_route(const char *route, int (*handler)(Request*)) {
    if(ROUTE_HANDLER_COUNT == WEBSERVER_MAXROUTES) return -1;
    ROUTE_HANDLER_COUNT++;
    struct RouteHandler *rhandler = &ROUTE_HANDLERS[ROUTE_HANDLER_COUNT-1];
    rhandler->route = route;
    rhandler->handler = handler;
    return ROUTE_HANDLER_COUNT;
}

struct RouteHandler* 
webserver_get_route(const char *route) {
    //printf("Looking for route [%s] [%d]\n", route, ROUTE_HANDLER_COUNT);
    for(int i = 0; i < ROUTE_HANDLER_COUNT; i++) {
        //printf("    Checking rount [%s]\n", ROUTE_HANDLERS[i].route);
        if(strcmp(route, ROUTE_HANDLERS[i].route) == 0) {
            //printf("Found route [%s]\n", route);
            return &ROUTE_HANDLERS[i];
        }
    }
    //printf("DID NOT Found route [%s]\n", route);
    return NULL;
}






int 
webserver_send404(int sfd) {
    char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
    write_string(sfd, response);
}

static int 
webserver_wait_for_socket(int fd) {
    fd_set wfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    /* Wait up to five seconds. */

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    retval = select(fd+1, NULL, &wfds, NULL, &tv);
    if (retval == -1)
        perror("select()");
    else if (retval) {
        printf("Data is available now.\n");
        return 0;
    } else {
        printf("No data within five seconds.\n");
        return -1;
    }
}

int 
webserver_send_file(int sfd, const char *filename) {

    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        perror("Failed to open file");
        return fd;
    }
    struct stat stat_buf; 
    fstat(fd, &stat_buf);

    write_string(sfd, "HTTP/1.1 200 OK\r\n");
    write_string(sfd, "Content-Type: text/html\r\n");
    write_string(sfd, "Content-Length: ");

    char length[16];
    sprintf(length, "%lu\r\n", stat_buf.st_size);	
    write_string(sfd, length);

    write_string(sfd, "\r\n");


    off_t offset = 0;
    for (size_t size_to_send = stat_buf.st_size; size_to_send > 0; )
    {
        //printf("                    SENDING BYTES [%lu] FROM [%lu] of [%lu]\n", size_to_send, offset, stat_buf.st_size);
        ssize_t sent = sendfile(sfd, fd, &offset, size_to_send);
        //printf("SENT [%lu]...\n", sent);

        if (sent == -1)
        {
            if(errno == EAGAIN) {
                //printf("EAGAIN, waiting for socket...\n");
                int r = webserver_wait_for_socket(sfd); 
                if(r == -1) {
                    perror("sendfile");
                    break;
                }
            }
            else if (sent != 0) {
                perror("sendfile");
                break;
            }
        } else if (sent == 0) {
            //printf("0 sent, waiting for socket...\n");
            int r = webserver_wait_for_socket(sfd); 
            if(r == -1) {
                perror("sendfile");
                break;
            }

        }

        size_to_send = stat_buf.st_size - offset;
    }

    //printf("					CLOSING [%d], sent [%lu]\n", sfd, offset);
    close(fd);
    return 0;
}


static int 
webserver_make_socket_non_blocking (int sfd) {
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
webserver_create_and_bind (const char *port) {
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
webserver_parse_request(Request *request) {


    printf("        Request [%s]\n", request->request);

    const char *verb = strtok(request->request, " :\r\n");
    strcpy(request->verb, verb);

    //printf("verb [%d]: %s\n", sfd, verb);

    if(strcmp(request->verb, "GET") == 0) {
        //printf("	This is a GET\n");
    } else {
        //printf("	Not a GET\n");
    }

    char *header_name;
    char *header_value;
    int is_websocket_upgrade = 0;
    char *websocket_key;

    char *route = strtok(NULL, " :\r\n");
    RouteHandler *handler = webserver_get_route(route);
    if(handler != NULL) {
        //printf("Doing route handler...\n\n");
        handler->handler(request);
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
                    //printf("####### Upgrade websockets!");
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
        websockets_accept(request->socketfd, websocket_key);
    } else {
        webserver_send404(request->socketfd);
    }
}

int 
webserver_init() {
    //bzero(ROUTE_HANDLERS, sizeof(struct RouteHandler) * MWEBSERVER_AXROUTES);
    websockets_init();
}

int 
webserver_start(char *port) {

    int sfd;
    int efd;
    int result;
    struct epoll_event event;
    struct epoll_event events[WEBSERVER_MAXEVENTS];

    struct Request request;


    sfd = webserver_create_and_bind (port);
    if (sfd == -1)
        abort ();

    result = webserver_make_socket_non_blocking (sfd);
    if (result == -1)
        abort ();

    result = listen (sfd, SOMAXCONN);
    if (result == -1)
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
    result = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
    if (result == -1)
    {
        perror ("epoll_ctl");
        abort ();
    }


    WEBSERVER_RUNNING = 1;

    /* The event loop */
    while (WEBSERVER_RUNNING)
    {
        printf("Listening...\n");
        int n = epoll_wait (efd, events, WEBSERVER_MAXEVENTS, -1);
        for (int i = 0; i < n; i++)
        {
            //printf("fd=[%d] n=[%d]\n", events[i].data.fd, n);

            if(events[i].events & EPOLLRDHUP) {
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
                //printf("					CLOSING [%d]\n", events[i].data.fd);
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

                    result = getnameinfo (&in_addr, in_len,
                            hbuf, sizeof hbuf,
                            sbuf, sizeof sbuf,
                            NI_NUMERICHOST | NI_NUMERICSERV);
                    if (result == 0)
                    {
                        //printf("Accepted connection on descriptor %d (host=%s, port=%s)\n", infd, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    result = webserver_make_socket_non_blocking (infd);
                    if (result == -1)
                        abort ();

                    event.data.fd = infd;
                    //printf("#####New connection fd [%d]\n", event.data.fd);
                    event.events = EPOLLIN | EPOLLET;
                    result = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
                    if (result == -1)
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


                    //printf("Reading from [%d]\n", events[i].data.fd);
                    count = read(events[i].data.fd, request.request, WEBSERVER_MAXREQUEST_LEN);
                    if(count == WEBSERVER_MAXREQUEST_LEN) {
                        printf("WARNING: Read MAXREQUEST_LEN [%d]\n", WEBSERVER_MAXREQUEST_LEN);
                    }
                    //printf("	Read [%ld]\n", count);
                    //printf("	Read from [%d]\n", events[i].data.fd);
                    if (count == -1)
                    {
                        if(errno == EBADF) {
                            done = 1;
                        } else if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
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

                    request.socketfd = events[i].data.fd;
                    request.length = count;

                    /* Write the buffer to standard output */
                    //s = write (1, buf, count);
                    //if (s == -1)
                    //{
                    //	perror ("write");
                    //	abort ();
                    //}

                    if(websockets_is_websocket(request.socketfd)) {
                        websockets_parse_request(request.socketfd, request.request, count);
                    } else {
                        webserver_parse_request(&request);
                    }
                }

                if (done)
                {
                    //printf ("Closing connection [%d]\n", events[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    if(websockets_is_websocket(events[i].data.fd) == 1) websockets_remove(events[i].data.fd);
                    //printf("					CLOSING [%d]\n", events[i].data.fd);
                    close (events[i].data.fd);
                }
            }
        }
    }


    printf("					CLOSING WEBSERVER [%d]\n", sfd);
    close (sfd);

}


