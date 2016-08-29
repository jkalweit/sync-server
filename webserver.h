#ifndef WEBSERVER_H
#define WEBSERVER_H

#define MAXEVENTS 64


int sync_start_server(char *port);
static int send404(int sfd);
static int send_file(int sfd, const char *filename);
static int make_socket_non_blocking (int sfd);
static int create_and_bind (const char *port);
static int parse_request(char *request, int sfd);

#endif
