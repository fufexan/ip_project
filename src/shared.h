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
char *recv_all(int, unsigned int);
void send_all(int, char *, unsigned int);
char **split_http_response(char *, long);
void save_file(char *, unsigned int, char *);
void debug(const char *restrict, ...);
void error(const char *restrict, ...);
void perrno(const char *restrict, ...);
char *make_error_message(const char *, ...);
