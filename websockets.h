#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include "utils.h"

#define MAXCLIENTS 64
#define WEBSOCKETS_MAXCHANNELS 256
#define WEBSOCKETS_MAXCHANNEL_LEN 64

typedef struct WebsocketMessage {
    int sfd;
    const char *channel;
    const char *cmd;
    char *data; 
} WebsocketMessage;

typedef int (*websocket_channel_handler)(WebsocketMessage*);

typedef struct WebsocketChannel {
    char channel[WEBSOCKETS_MAXCHANNEL_LEN];
    websocket_channel_handler handler;
} WebsocketChannel;

static int send_test_message(const int sfd);
static unsigned int hash_sha1(const char* dataToHash, const size_t dataSize, unsigned char* outHashed);

int websockets_init();
void websockets_add(const int fd);
void websockets_remove(const int fd);
int websockets_accept(const int sfd, const char *client_key);
int websockets_is_websocket(const int fd);
int websockets_parse_request(const int sfd, unsigned char *received, const int received_len);
int websockets_send_message(const int sfd, const char *msg);
int websockets_broadcast(const char *msg);

int websockets_add_channel(const char *channel, const websocket_channel_handler handler);
websocket_channel_handler websocket_get_channel(const char *channel);



#endif
