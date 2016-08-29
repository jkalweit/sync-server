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
const char MAGIC_WEBSOCKET_STR[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
int clients[MAXCLIENTS];

int 
websockets_init() {
	bzero(clients, MAXCLIENTS);
	OpenSSL_add_all_algorithms();	
}


static int 
send_test_message(int sfd) {
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
websockets_send_message(int sfd, char *msg) {

	int str_len = strlen(msg);

	printf("			Sending [%d]: [%s]\n", sfd, msg);

	char message[1028];
	message[0] = 0x81;
	message[1] = (char) str_len;
	for(int i = 0; i < str_len; i++) {	
		message[i+2] = msg[i];
	}

	//print_bytes(message, str_len+2);	
	int n = write(sfd,message,str_len+2);
	//printf("Wrote %d bytes.\n", n);
	if (n < 0) error("ERROR writing msg to socket");
}

static unsigned 
int hash_sha1(const char* dataToHash, size_t dataSize, unsigned char* outHashed) {
	unsigned int md_len = -1;
	const EVP_MD *md = EVP_get_digestbyname("SHA1");
	if(NULL != md) {
		EVP_MD_CTX mdctx;
		EVP_MD_CTX_init(&mdctx);
		EVP_DigestInit_ex(&mdctx, md, NULL);
		EVP_DigestUpdate(&mdctx, dataToHash, dataSize);
		EVP_DigestFinal_ex(&mdctx, outHashed, &md_len);
		EVP_MD_CTX_cleanup(&mdctx);
	}
	return md_len;
}

void 
websockets_remove(int fd) {
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
websockets_add(int fd) {
	printf("add [%d]\n", fd);
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == 0) {
			clients[i] = fd;
			printf("added [%d]\n", fd);
			return;
		}
	}	
}

int 
websockets_is_websocket(int fd) {
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == fd) {
			printf("IS a websocket [%d]\n", fd);
			return 1;
		}
	}	
	printf("IS NOT a websocket [%d]\n", fd);
	return 0;
}

int 
websockets_parse_request(unsigned char *received, int n, int sfd) {
	
	printf("OpCode [%d] ", sfd);
	print_bytes(received, 1);
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
	//sleep(1);
	//send_test_message(data->fd);

	printf("Received bytes: %d\n", n); //, Type: 0x%02X; Size: 0x%02x\n", n, received[0], received[1]); 
	received[n] = '\0';
	print_bytes(received, n+1);

	char data_length = n-6; //received[2] & 127;

	mask[0] = received[2];	
	mask[1] = received[3];	
	mask[2] = received[4];	
	mask[3] = received[5];	

	//print_bytes(mask, 4);

	for(int i = 0; i < data_length; i++) {
		received[i+6] = received[i+6] ^ mask[i%4];
	}

	printf("Decoded bytes: %d\n", n); //, Type: 0x%02X; Size: 0x%02x\n", n, received[0], received[1]); 
	print_bytes(received, n+1);
	printf("	Decoded [%d]: %s\n", sfd, &received[6]);
	print_bytes(&received[6], n-6);

	char echo[256] = "Echo: ";
	strcat(echo, &received[6]);

	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] != 0) {
			printf("		Echoing to [%d]\n", clients[i]);
			websockets_send_message(clients[i], echo);
		}
	}	
}


int 
websockets_accept(int sfd, char *client_key) {

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

	printf("##Accepted websocket: fd [%d] is_websocket [%d] \n", sfd, websockets_is_websocket(sfd)); 

	//sleep(1);
	//send_test_message(sfd);
}



