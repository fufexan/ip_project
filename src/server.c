#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "./shared.h"

#define TEAM_PORT "22034"

void *handle_connection(void *);

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
    error("Error occurred:%s\n", gai_strerror(status));
    exit(1);
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perrno("Could not create socket!");
    exit(1);
  }
  debug("Socket created");

  // Reuse previous address in case the server was restarted
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    perrno("setsockopt error!");
    exit(1);
  }

  debug("Set socket to reuse previous addres\n");
  // Bind the socket to our port (set by calling getaddrinfo)
  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    perrno("bind error!");
    exit(1);
  }

  freeaddrinfo(res);

  debug("Listening for connections...");
  // Listen for incoming connections (max 10 at a time)
  if (listen(sockfd, 10) == -1) {
    perrno("listen error!");
    exit(1);
  }

  // Get ready to accept a connection
  int newsockfd;
  struct sockaddr_storage remote_addr;
  socklen_t addr_size;

  while (true) {
    // Blocks until a connection is initiated
    debug("Accepting connection...");
    if ((newsockfd = accept(sockfd, (struct sockaddr *)&remote_addr,
                            &addr_size)) == -1) {
      perrno("accept error!");
      exit(1);
    }

    char remote_ipv4[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &remote_addr, remote_ipv4, INET_ADDRSTRLEN) ==
        NULL) {
      error("Failed to get string representation of remote address");
    }

    printf("New connection from %s\n", remote_ipv4);

    // Make a pthread
    pthread_t t;
    int *pclient = malloc(sizeof(int));
    *pclient = newsockfd;
    pthread_create(&t, NULL, handle_connection, pclient);
  }

  printf("Closing server\n");
  close(sockfd);
  return 0;
}

void *handle_connection(void *fd) {
  int newsockfd = *(int *)fd;
  free(fd);
  debug("Connection accepted. Waiting for messages...");

  char *buf;
  // Keep connection open as long as the client is connected
  while (strlen(buf = receive(newsockfd, 3)) != 0) {
    // We only want 3 bytes, in the form "xy#", where x and y are digits
    int cmd = atoi(buf);
    printf("cmd: %s\n", buf);
    // free(buf);
    char *response_buf = malloc(sizeof(char) * 25);
    if (cmd != 4) {
      strcpy(response_buf, "Command not implemented\n");
    } else {
      strcpy(response_buf, "Command implemented\n");
    }

    send(newsockfd, response_buf, strlen(response_buf), 0);
  }

  debug("Closing connection");

  free(buf);
  close(newsockfd);
  pthread_exit(0);
  return NULL;
}
