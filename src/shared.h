#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int check(int, const char *);
void *malloc_s(size_t);
void *realloc_s(void *, size_t);
void *get_in_addr(struct sockaddr *);
char *recv_all(int sockfd, unsigned int num_bytes);
void send_all(int sockfd, char *buf, unsigned int num_bytes);
char **split_http_response(char *buf, long len);
void save_file(char *buffer, unsigned int length, char *file_name);
void debug(const char *restrict, ...);
void error(const char *restrict, ...);
void perrno(const char *restrict, ...);
