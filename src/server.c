#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "shared.h"

#define PORT "22034"
#define ASSIGNED_COMMAND 4

void *handle_connection(void *);
int get_listener_socket(void);

int sockfd;

void int_handler(int sig) {
  check(close(sockfd), "close");
  printf("\nServer closed\n");

  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGINT, int_handler);
  printf("Starting IPv4 server...\n");

  sockfd = get_listener_socket();

  // Get ready to accept a connection
  int newsockfd;
  struct sockaddr_storage remote_addr;
  socklen_t addr_size = sizeof(remote_addr);

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
    // pthreads want a void pointer, so we'll cast our fd. This will be freed in
    // handle_connection
    int *threadarg = malloc_s(sizeof(int));
    *threadarg = newsockfd;
    pthread_create(&t, NULL, handle_connection, threadarg);
    // Threads are on their own
    pthread_detach(t);
  }

  return 0;
}

void *handle_connection(void *fd) {
  int newsockfd = *(int *)fd;
  free(fd);

  debug("Connection accepted. Waiting for messages...");

  char *buf = NULL, *response_buf = NULL;

  // Keep connection open as long as the client is connected
  while ((buf = recv_all(newsockfd, 3)) && strlen(buf) != 0) {
    // We only want 3 bytes, in the form "xy#", where x and y are digits
    int cmd = atoi(buf);
    printf("cmd: %s\n", buf);
    free(buf); // Free buf after use

    if (cmd != ASSIGNED_COMMAND) {
      response_buf = "Command not implemented";
    } else {
      // We receive allocated memory that we have to free
      response_buf = client(cmd);
    }

    // Send response
    send_all(newsockfd, response_buf, strlen(response_buf));

    // Free dynamically allocated response_buf
    if (cmd == ASSIGNED_COMMAND) {
      free(response_buf);
    }
  }

  debug("Closing connection");

  if (buf) {
    free(buf); // Free if recv_all returned non-NULL
  }

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
