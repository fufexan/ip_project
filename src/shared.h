#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

char *receive(int sockfd);
char **split_http_response(char *buf, long len);
void save_file(char *buffer, unsigned int length, char *file_name);
