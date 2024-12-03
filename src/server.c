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

// Hold information about the active sockets
typedef struct {
  // Array of active socket descriptors
  int *active_sockets;
  // Number of active sockets
  size_t socket_count;
} resource_tracker;

// Functions only used by the server
void *handle_connection(void *);
int get_listener_socket(void);
void track_sock(resource_tracker *, int);
void untrack_sock(resource_tracker *, int);
void close_all_sockets(resource_tracker *);
void int_handler(int);

// Global variables
const char PORT[] = "22034";
const int ASSIGNED_COMMAND = 4;
bool ALL_COMMANDS = false;
bool LOCALHOST = false;
resource_tracker tracker = {NULL, 0};

// Main program, runs the server which accepts multiple connections and handles
// them in parallel
int main(int argc, char **argv) {
  // Add signal handlers to gracefully close on exit
  signal(SIGINT, int_handler);
  signal(SIGTERM, int_handler);

  printf("Starting IPv4 server...\n");

  // If env var ALL_COMMANDS=1 is present, enable all commands, instead of only
  // ASSIGNED_COMMAND
  ALL_COMMANDS = getenv("ALL_COMMANDS") != NULL;

  // If env var LOCALHOST=1 is present, send a request to localhost instead of
  // ASSIGNED_COMMAND
  LOCALHOST = getenv("LOCALHOST") != NULL;

  // Get a socket to listen for new connections
  int sockfd = get_listener_socket();
  if (sockfd < 0) {
    perrno("Could not bind the listening socket");
    exit(1);
  }

  // Track the socket
  track_sock(&tracker, sockfd);

  // Get ready to accept a connection
  int client_fd;
  struct sockaddr_storage remote_addr;
  socklen_t addr_size = sizeof(remote_addr);

  while (true) {
    // Blocks until a connection is initiated
    debug("Accepting connection...");
    client_fd =
        check(accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size),
              "Could not accept connection");

    // Add client_fd to tracker's active sockets
    track_sock(&tracker, client_fd);

    // Get string representation of the client's IP
    char remote_ipv4[INET_ADDRSTRLEN];

    if (!inet_ntop(remote_addr.ss_family,
                   get_in_addr((struct sockaddr *)&remote_addr), remote_ipv4,
                   INET_ADDRSTRLEN)) {
      error("Failed to get string representation of remote address");
    }

    printf("New connection from %s on socket %d\n", remote_ipv4, client_fd);

    // Make a pthread to handle the connection in parallel with others
    pthread_t t;
    // pthreads want the argument as void *, so we'll cast our fd. This will be
    // freed in handle_connection
    int *threadarg = malloc_s(sizeof(int));
    *threadarg = client_fd;
    pthread_create(&t, NULL, handle_connection, threadarg);

    // Threads are on their own
    // Even though we detach the thread immediately after creation, valgrind may
    // still falsely report the thread's local memory as a leak, even though it
    // isn't.
    pthread_detach(t);
  }

  return 0;
}

// Handle connections initiated by clients. Can be used with pthreads.
void *handle_connection(void *fd) {
  // Cast the argument to its proper type and free the memory
  int client_fd = *(int *)fd;
  free(fd);

  debug("Connection accepted. Waiting for messages...");

  char *buf = NULL, *response_buf = NULL;

  // Keep connection open as long as the client is connected
  while ((buf = recv_all(client_fd, 3)) && strlen(buf) != 0) {
    // We only want 3 bytes, in the form "xy#", where x and y are digits
    int cmd = atoi(buf);
    printf("cmd: %s\n", buf);
    free(buf); // Free buf after use

    // If the server is configured to only respond to the assigned command, and
    // the received command is not that, return "Command not implemented"
    if (!ALL_COMMANDS && cmd != ASSIGNED_COMMAND) {
      response_buf = make_error_message("Command not implemented");
    } else {
      // Use localhost (cmd 0) instead of ASSIGNED_COMMAND if LOCALHOST is true
      if (LOCALHOST && cmd == ASSIGNED_COMMAND) {
        cmd = 0;
      }
      // We receive allocated memory that we have to free
      response_buf = client(cmd);
    }

    // Send response
    send_all(client_fd, response_buf, strlen(response_buf));

    // Free dynamically allocated response_buf
    free(response_buf);
  }

  debug("Closing connection");

  // Free buffer if recv_all returned non-NULL
  if (buf) {
    free(buf);
  }

  // Close buffer
  check(close(client_fd), "close");
  // Untrack socket
  untrack_sock(&tracker, client_fd);
  // Exit pthread
  pthread_exit(0);
  return NULL;
}

// Return a listening socket or -1 in case of error
int get_listener_socket(void) {
  int listener, yes = 1, rv;

  struct addrinfo hints, *ai, *p;

  // Zero-init hints
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
      check(close(listener), "close");
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

// Close all sockets in a resource_tracker struct
void close_all_sockets(resource_tracker *tracker) {
  for (size_t i = 0; i < tracker->socket_count; i++) {
    check(close(tracker->active_sockets[i]), "close");
  }

  free(tracker->active_sockets);
  tracker->active_sockets = NULL;
}

// SIGINT handler
void int_handler(int sig) {
  close_all_sockets(&tracker);
  printf("\nServer closed\n");

  exit(0);
}

// Track a socket inside a resource_tracker struct
void track_sock(resource_tracker *tracker, int fd) {
  tracker->active_sockets = realloc_s(
      tracker->active_sockets, (tracker->socket_count + 1) * sizeof(int));
  tracker->active_sockets[tracker->socket_count++] = fd;
}

// Remove a socket from a resource_tracker
void untrack_sock(resource_tracker *tracker, int fd) {
  // Loop through all the active sockets
  for (size_t i = 0; i < tracker->socket_count; i++) {
    if (tracker->active_sockets[i] != fd) {
      continue;
    }

    // If we found our socket, copy the last element over it and decrement the
    // socket count, then exit the loop
    tracker->active_sockets[i] =
        tracker->active_sockets[--tracker->socket_count];
    break;
  }
}
