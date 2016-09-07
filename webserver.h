#ifndef WEBSERVER_H
#define WEBSERVER_H

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


#include "utils.h"
#include "websockets.h"




#define WEBSERVER_MAXEVENTS 64
#define WEBSERVER_MAXROUTES 128
#define WEBSERVER_MAXREQUEST_LEN 100000

volatile int WEBSERVER_RUNNING;

typedef struct Request {
    int socketfd;
    char request[WEBSERVER_MAXREQUEST_LEN]; // full request buffer
    char verb[16];
    char route[1024];
    int length;
} Request;

typedef struct RouteHandler {
    const char *route;
    int (*handler)(Request*);
} RouteHandler;



int webserver_init();
int webserver_add_route(const char *route, int (*handler)(Request*));
struct RouteHandler* webserver_get_route(const char *route);

int webserver_start(char *port);
int webserver_send404(int sfd);
int webserver_send_file(int sfd, const char *filename);
static int webserver_make_socket_non_blocking (int sfd);
static int webserver_create_and_bind (const char *port);
static int webserver_parse_request(Request *request);

#endif
