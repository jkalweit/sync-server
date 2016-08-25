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
#include <openssl/bio.h>
#include <openssl/evp.h>

#define MAXEVENTS 64
#define MAXCLIENTS 64

const char MAGIC_WEBSOCKET_STR[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int clients[MAXCLIENTS];


void error(const char *msg)
{
	perror(msg);
	exit(1);
}

int write_string(int fd, char* string) {
	int n = write(fd,string,strlen(string));
	if (n < 0) error("ERROR writing to socket");
}


char* skip_chars(char *start, char chars[]) {
	char *result = start;
	while(strchr(chars, result[0]) != NULL && result[0] != '\0') result++;
	return result;
}

int send404(int sfd) {
	char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
	write_string(sfd, response);
}

int send_file(int sfd, const char *filename) {

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


static int make_socket_non_blocking (int sfd)
{
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

static int create_and_bind (const char *port)
{
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


void print_bytes(const void *object, size_t size)
{
	const unsigned char * const bytes = object;
	size_t i;

	printf("[ ");
	for(i = 0; i < size; i++)
	{
		printf("%02x ", bytes[i]);
	}
	printf("]\n");
}



int send_test_message(int sfd) {
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

int websocket_send_message(int sfd, char *msg) {

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

unsigned int hash_sha1(const char* dataToHash, size_t dataSize, unsigned char* outHashed) {
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

void remove_websocket(int fd) {
	printf("Remove [%d]\n", fd);
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == fd) {
			clients[i] = 0;			
			printf("Removed [%d]\n", fd);
			return;
		}
	}
}

void add_websocket(int fd) {
	printf("add [%d]\n", fd);
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == 0) {
			clients[i] = fd;
			printf("added [%d]\n", fd);
			return;
		}
	}	
}

int is_websocket(int fd) {
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i] == fd) {
			printf("IS a websocket [%d]\n", fd);
			return 1;
		}
	}	
	printf("IS NOT a websocket [%d]\n", fd);
	return 0;
}

int accept_websocket(int sfd, char *client_key) {

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

	add_websocket(sfd);

	printf("##Accepted websocket: fd [%d] is_websocket [%d] \n", sfd, is_websocket(sfd)); 

	//sleep(1);
	//send_test_message(sfd);
}


int parse_request_websocket(unsigned char *received, int n, int sfd) {
	
	printf("OpCode [%d] ", sfd);
	print_bytes(received, 1);
	if(received[0] == 0x88) {
		printf("Client request close websocket [%d]\n", sfd);
		remove_websocket(sfd);
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
			websocket_send_message(clients[i], echo);
		}
	}	
}



int parse_request(char *request, int sfd) {


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
		accept_websocket(sfd, websocket_key);
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

int main (int argc, char *argv[])
{
	int sfd, s, efd;
	struct epoll_event event;
	struct epoll_event *events;


	bzero(clients, MAXCLIENTS);
	OpenSSL_add_all_algorithms();	


	sfd = create_and_bind ("8080");
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


				if(is_websocket(events[i].data.fd)) remove_websocket(events[i].data.fd);
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

					if(is_websocket(events[i].data.fd)) {
						printf("is_websocket: [%d]\n", is_websocket(events[i].data.fd));
						parse_request_websocket(buf, count, events[i].data.fd);
					} else {
						parse_request(buf, events[i].data.fd);
					}
				}

				if (done)
				{
					printf ("Closing connection [%d]\n", events[i].data.fd);

					/* Closing the descriptor will make epoll remove it
					   from the set of descriptors which are monitored. */
					if(is_websocket(events[i].data.fd) == 1) remove_websocket(events[i].data.fd);
					printf("					CLOSING [%d]\n", events[i].data.fd);
					close (events[i].data.fd);
				}
			}
		}
	}

	free (events);


	printf("					CLOSING [%d]\n", sfd);
	close (sfd);

	return EXIT_SUCCESS;
}
