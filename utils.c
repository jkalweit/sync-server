#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


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



