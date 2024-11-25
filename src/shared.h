#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *get_in_addr(struct sockaddr *);
char *receive(int sockfd, unsigned int num_bytes);
char **split_http_response(char *buf, long len);
void save_file(char *buffer, unsigned int length, char *file_name);
void debug(const char *restrict, ...);
void error(const char *restrict, ...);
void perrno(const char *restrict, ...);
