#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

static int send_test_message(int sfd);
static unsigned int hash_sha1(const char* dataToHash, size_t dataSize, unsigned char* outHashed);

int websockets_init();
void websockets_add(int fd);
void websockets_remove(int fd);
int websockets_accept(int sfd, char *client_key);
int websockets_is_websocket(int fd);
int websockets_parse_request(unsigned char *received, int n, int sfd);
int websockets_send_message(int sfd, char *msg);



#endif
