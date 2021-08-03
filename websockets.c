#include "websockets.h"


static const char MAGIC_WEBSOCKET_STR[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static int clients[MAXCLIENTS];
static WebsocketChannel channel_handlers[WEBSOCKETS_MAXCHANNELS];

static int channel_count = 0;

int 
websockets_init() {
	bzero(clients, MAXCLIENTS);
	//bzero(channel_handlers, WEBSOCKETS_MAXCHANNELS);
	OpenSSL_add_all_algorithms();	
}


static int 
send_test_message(const int sfd) {
	char message[6];
	message[0] = 0x81;
	message[1] = 0x04;
	message[2] = 'T';
	message[3] = 'E';
	message[4] = 'S';
	message[5] = 'T';

	int n = write(sfd,message,6);
	if (n < 0) error("ERROR writing TEST to socket");
}

int 
websockets_send_message(const int sfd, const char *msg) {

	size_t str_len = strlen(msg);

	printf("			Sending [%d] [%li]: [%s]\n", sfd, str_len, msg);

    char message[1028];
	message[0] = 0x81;
    int data_offset;
    if (str_len <= 125) {
	    message[1] = (char) str_len;
        data_offset = 2;
    } else if (str_len < 65535) {
        message[1] = (char) 126;
        message[2] = (char) (str_len >> 8) & 255;
        message[3] = (char) (str_len     ) & 255;
        data_offset = 4;
    } else  {
        message[1] = (char) 127;
        message[2] = (char) (str_len >> 56) & 255;
        message[3] = (char) (str_len >> 48) & 255;
        message[4] = (char) (str_len >> 40) & 255;
        message[5] = (char) (str_len >> 32) & 255;
        message[6] = (char) (str_len >> 24) & 255;
        message[7] = (char) (str_len >> 16) & 255;
        message[8] = (char) (str_len >>  8) & 255;
        message[9] = (char) (str_len      ) & 255;
        data_offset = 10;
    }

	for(unsigned int i = 0; i < str_len; i++) {	
		message[i+data_offset] = msg[i];
	}

	int n = write(sfd,message,str_len+data_offset);
	if (n < 0) error("ERROR writing msg to socket");
}

static unsigned int 
hash_sha1(const char* dataToHash, const size_t dataSize, unsigned char* outHashed) {
	unsigned int md_len = -1;
	const EVP_MD *md = EVP_get_digestbyname("SHA1");
	EVP_MD_CTX *mdctx;
	mdctx = EVP_MD_CTX_create();
	if(NULL != md) {
		EVP_MD_CTX_init(mdctx);
		EVP_DigestInit_ex(mdctx, md, NULL);
		EVP_DigestUpdate(mdctx, dataToHash, dataSize);
		EVP_DigestFinal_ex(mdctx, outHashed, &md_len);
		EVP_MD_CTX_destroy(mdctx);
	}
	return md_len;
}

void 
websockets_remove(const int fd) {
	printf("Remove [%d]\n", fd);
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == fd) {
			clients[i] = 0;			
			printf("Removed [%d]\n", fd);
			return;
		}
	}
}

void 
websockets_add(const int fd) {
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == 0) {
			clients[i] = fd;
			return;
		}
	}	
}

int 
websockets_is_websocket(const int fd) {
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == fd) {
			return 1;
		}
	}	
	return 0;
}

int 
websockets_parse_request(const int sfd, unsigned char *received, const int received_len) {
	
	//printf("OpCode [%d] ", sfd);
	//print_bytes(received, 1);
	if(received[0] == 0x88) {
		printf("Client request close websocket [%d]\n", sfd);
		websockets_remove(sfd);
		//unsigned char close_msg[2];
		//close_msg[0] = 0x88;
		//close_msg[1] = 0x00;
		//int t = write(data->fd,close_msg,2);
		//if (t < 0) error("ERROR writing msg to socket");

		printf("					CLOSING [%d]\n", sfd);
		close(sfd);
		return 0;
	}
	

	char mask[4];
	//printf("Received bytes: %d\n", received_len); //, Type: 0x%02X; Size: 0x%02x\n", n, received[0], received[1]); 
	received[received_len] = '\0';
	//print_bytes(received, received_len+1);

	char data_length = received_len-6; //received[2] & 127;

	mask[0] = received[2];	
	mask[1] = received[3];	
	mask[2] = received[4];	
	mask[3] = received[5];	

	//print_bytes(mask, 4);

	for(int i = 0; i < data_length; i++) {
		received[i+6] = received[i+6] ^ mask[i%4];
	}

	//printf("	Decoded [%d]: %s\n", sfd, &received[6]);
	//print_bytes(&received[6], received_len-6);


    char *channel = strtok(&received[6], " ");
    char *cmd = strtok(NULL, " ");
    char *data = strtok(NULL, "");

    //printf("Channel [%s] Data: [%s]\n", channel, data);


    websocket_channel_handler handler = websocket_get_channel(channel);
    if(handler != 0) {
        WebsocketMessage msg;
        msg.sfd = sfd;
        msg.channel = channel;
        msg.cmd = cmd;
        msg.data = data;
        handler(&msg);
    }
}

int
websockets_broadcast(const char *msg) {
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] != 0) {
			printf("		Sending to [%d]\n", clients[i]);
			websockets_send_message(clients[i], msg);
		}
	}	
}


int 
websockets_accept(const int sfd, const char *client_key) {

    printf("Accepting....[%d]\n", sfd);
	int key_size = 1028;
	unsigned char key[key_size];
	const size_t SHA_DIGEST_LENGTH_HEX = 40;	
	const size_t SHA_DIGEST_LENGTH_BYTES = SHA_DIGEST_LENGTH_HEX / 2;	

	strcpy(key, client_key); 
	//strcpy(key, "dGhlIHNhbXBsZSBub25jZQ=="); //client_key); 
	// for testing: "dGhlIHNhbXBsZSBub25jZQ==" 
	// Sha1 hash: b37a4f2cc0624f1690f64606cf385945b2bec4ea
	// Base64 bytes should be 733370504c4d426954786151396b59477a7a685a52624b2b784f6f3d
	// Base64 accept should be "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="

	//printf("key:[%s]\n", key);
	strcat(key, MAGIC_WEBSOCKET_STR);
	//printf("key:[%s] len:[%lu]\n", key, strlen(key));

	unsigned char hash[SHA_DIGEST_LENGTH_BYTES*3];
	bzero(hash, SHA_DIGEST_LENGTH_BYTES*3);
	hash_sha1(key, strlen(key), hash);
	//hash[SHA_DIGEST_LENGTH/2] = '\0';
	//printf("SHA1 Hash %lu: ", SHA_DIGEST_LENGTH_BYTES);
	//print_bytes(hash, SHA_DIGEST_LENGTH_BYTES);

	unsigned char encoded[key_size];
	bzero(encoded, key_size);
	FILE* stream = fmemopen(encoded, key_size, "w");
	BIO *bio, *b64;
	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	bio = BIO_new_fp(stream, BIO_NOCLOSE);
	BIO_push(b64, bio);
	BIO_write(b64, hash, SHA_DIGEST_LENGTH_BYTES);
	BIO_flush(b64);
	BIO_free_all(b64); 
	fclose(stream);
	//encoded[SHA_DIGEST_LENGTH/2] = '\0'; // null terminate to handle as string -JDK 2016-08-24
	//printf("Base64 encoded:[%s]\n", encoded);

	//print_bytes(encoded, strlen(encoded));

	char accept[1028] = "Sec-WebSocket-Accept: ";
	strcat(accept, encoded);
	//printf("accept:[%s]\n", accept);

	write_string(sfd, "HTTP/1.1 101 Web Socket Protocol Handshake\r\n");
	write_string(sfd, "Upgrade: WebSocket\r\n");
	write_string(sfd, "Connection: Upgrade\r\n");
	write_string(sfd, accept);
	write_string(sfd, "\r\n");
	write_string(sfd, "Sec-WebSocket-Version: 13\r\n");
	write_string(sfd, "\r\n");

	websockets_add(sfd);
    
    printf("Accepted [%d]\n", sfd);

	//printf("##Accepted websocket: fd [%d] is_websocket [%d] \n", sfd, websockets_is_websocket(sfd)); 

	//sleep(1);
	//send_test_message(sfd);
}



int 
websockets_add_channel(const char *channel, const websocket_channel_handler handler) {
    if(channel_count == WEBSOCKETS_MAXCHANNELS) return -1;
    int channel_len = strlen(channel);
    if(channel_len > WEBSOCKETS_MAXCHANNEL_LEN-1) return -1;
    WebsocketChannel *channel_handler = &channel_handlers[channel_count];
    strcpy(channel_handler->channel, channel);
    channel_handler->channel[channel_len] = '\0';
    channel_handler->handler = handler;
    //printf("Added channel [%p] [%d] [%s]\n", &channel_handler->channel, channel_count, channel_handler->channel);
    //print_bytes(channel_handler->channel, 5);
    //print_bytes("db", 3);
    channel_count++;
    return channel_count;
}

websocket_channel_handler 
websocket_get_channel(const char *channel) {
    //printf("Looking for channel [%d] [%s]\n", channel_count, channel);
    for(int i = 0; i < channel_count; i++) {
        WebsocketChannel *c = &channel_handlers[i];
        //printf("Comparing channel [%p] [%d] [%s]\n", c->channel, i, c->channel);
        //print_bytes(c->channel, 5);
        if(strcmp(channel, c->channel) == 0) {
            //printf("Found channel [%s]\n", channel);
            return c->handler;
        }
    }
    printf("DID NOT find channel [%s]\n", channel);
    return NULL;
}
