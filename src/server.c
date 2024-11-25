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

#define PORT "22034"

void *handle_connection(void *);
int get_listener_socket(void);

int main(int argc, char **argv) {
  printf("Starting IPv4 server...\n");

  int sockfd = get_listener_socket();

  // Get ready to accept a connection
  int newsockfd;
  struct sockaddr_storage remote_addr;
  socklen_t addr_size;

  while (true) {
    // Blocks until a connection is initiated
    debug("Accepting connection...");
    newsockfd =
        check(accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size),
              "accept error!");

    char remote_ipv4[INET_ADDRSTRLEN];

    if (!inet_ntop(remote_addr.ss_family,
                   get_in_addr((struct sockaddr *)&remote_addr), remote_ipv4,
                   INET_ADDRSTRLEN)) {
      error("Failed to get string representation of remote address");
    }

    printf("New connection from %s on socket %d\n", remote_ipv4, newsockfd);

    // Make a pthread
    pthread_t t;
    int *threadarg = malloc(sizeof(int));
    *threadarg = newsockfd;
    pthread_create(&t, NULL, handle_connection, threadarg);
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

// Return a listening socket
int get_listener_socket(void) {
  int listener, yes = 1, rv;

  struct addrinfo hints, *ai, *p;

  memset(&hints, 0, sizeof hints);
  // IPv4
  hints.ai_family = AF_INET;
  // TCP socket
  hints.ai_socktype = SOCK_STREAM;
  // Fill in the address automatically and set the service literally
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

  // Get address info (NULL -> localhost)
  if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
    fprintf(stderr, "pollserver: %s\n", gai_strerror(rv));
    exit(1);
  }

  // Find the first IPv4 address and try to bind to it
  for (p = ai; p != NULL; p = p->ai_next) {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0) {
      continue;
    }

    // Get the same port even after a restart of the server
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // Bind to requested port
    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
      close(listener);
      continue;
    }

    break;
  }

  // Free up address, we're done with it
  freeaddrinfo(ai);

  // If we got NULL here we didn't get bound
  if (p == NULL) {
    return -1;
  }

  // Listen
  check(listen(listener, 10), "Could not start listening");

  return listener;
}
