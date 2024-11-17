#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "./shared.h"

#define TEAM_PORT "22034"

int main(int argc, char **argv) {
  printf("Starting IPv4 server...\n");

  struct addrinfo hints, *res;
  int sockfd, status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // Use IPv4
  hints.ai_socktype = SOCK_STREAM; // Use TCP
  hints.ai_flags =
      AI_PASSIVE | AI_NUMERICSERV; // Fill in my IP and use service verbatim

  if ((status = getaddrinfo(NULL, TEAM_PORT, &hints, &res)) != 0) {
    fprintf(stderr, "Error occurred:%s\n", gai_strerror(status));
    exit(1);
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    fprintf(stderr, "Could not create socket!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }
  printf("Socket created\n");

  // Reuse previous address in case the server was restarted
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    fprintf(stderr, "setsockopt error!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("Set socket to reuse previous addres\n");
  // Bind the socket to our port (set by calling getaddrinfo)
  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    fprintf(stderr, "bind error!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("Listening for connections...\n");
  // Listen for incoming connections (max 10 at a time)
  if (listen(sockfd, 10) == -1) {
    fprintf(stderr, "listen error!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  // Get ready to accept a connection
  int newsockfd;
  struct sockaddr_storage remote_addr;
  socklen_t addr_size;

  // Blocks until the first connection is made
  // printf("Accepting connection...\n");
  if ((newsockfd =
           accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size)) == -1) {
    fprintf(stderr, "accept error!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("Connection accepted\nWaiting for messages...\n");

  char *buf;
  // Keep connection open as long as the client is connected
  while (strlen(buf = receive(newsockfd, 3)) != 0) {
    // We only want 3 bytes, in the form "xy#", where x and y are digits
    int cmd = atoi(buf);
    printf("cmd: %s\n", buf);
    char *response_buf = malloc(sizeof(char) * 25);
    if (cmd != 4) {
      strcpy(response_buf, "Command not implemented");
    } else {
      strcpy(response_buf, "Command implemented");
    }

    send(newsockfd, response_buf, strlen(response_buf), 0);
  }

  printf("Closing server\n");

  free(buf);
  close(newsockfd);
  close(sockfd);
  freeaddrinfo(res);
  return 0;
}
