#ifndef UTILS_H
#define UTILS_H

void error(const char *msg);
int write_string(int fd, char* string);
char* skip_chars(char *start, char chars[]);
void print_bytes(const void *object, size_t size);

#endif
