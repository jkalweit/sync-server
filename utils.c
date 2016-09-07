#include "utils.h"


void error(const char *msg)
{
	perror(msg);
	exit(1);
}

char*
copy_string(const char *src) {
    size_t len = strlen(src);
    char *copy = malloc((len+1) * sizeof(char));
    strcpy(copy, src);
    return copy;
}

int write_string(const int fd, const char* string) {
	int n = write(fd,string,strlen(string));
	if (n < 0) error("ERROR writing to socket");
}


char* skip_chars(char *start, const char chars[]) {
	char *result = start;
	while(strchr(chars, result[0]) != NULL && result[0] != '\0') result++;
	return result;
}


void print_bytes(const void *object, const size_t size)
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



