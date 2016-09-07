#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


void error(const char *msg);
char* copy_string(const char *src);
int write_string(const int fd, const char* string);
char* skip_chars(char *start, const char chars[]);
void print_bytes(const void *object, const size_t size);

#endif
