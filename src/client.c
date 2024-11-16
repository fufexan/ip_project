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

#include "./destinations.h"

struct addrinfo *get_ipv6_for(const char *name, const char *service) {
  struct addrinfo hints, *res;
  int status;

  memset(&hints, 0, sizeof hints); // zero-init the struct
  hints.ai_family = AF_INET6;      // we only want IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // autofill IP

  if ((status = getaddrinfo(name, "http", &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  }

  return res;
}

void save_file(char *buffer, unsigned int length, char *file_name) {
  FILE *file = fopen(file_name, "w");

  if (file) {
    fwrite(buffer, sizeof(char), length, file);
  }

  fclose(file);
}

int main(int argc, char **argv) {
  printf("Starting IPv6 client...\n");

  // 64 bytes should be enough
  char *host = calloc(64, sizeof(char));
  char hostaddr[INET6_ADDRSTRLEN];
  bool addr_found = false;
  int sockfd;

  // Default host
  strcpy(host, "google.com");

  if (argc > 1) {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < DEST_MAX + 1) {
      strcpy(host, destinations[atoi(argv[1])]);
    } else {
      fprintf(stderr,
              "Invalid hostname! Please pick a number between 0 and %d\n",
              DEST_MAX);

      printf("Using host %s", hostaddr);
    }
  }

  struct addrinfo *res = get_ipv6_for(host, "http");
  struct addrinfo *p; // iterator

  printf("IPv6 address of %s:\n", host);

  // Check all results, stop on first valid address
  for (p = res; p != NULL && !addr_found; p = p->ai_next) {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    struct in6_addr *addr = &(ipv6->sin6_addr);

    // convert the IP to a string and print it
    const char *ptr = inet_ntop(AF_INET6, addr, hostaddr, sizeof hostaddr);

    if (ptr == NULL) {
      fprintf(stderr, "Failed to get string representation of IPv6 address!\n");
      fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    } else {
      printf("%s\n", hostaddr);
      addr_found = true;
    }
  }

  if (!addr_found) {
    fprintf(stderr, "Could not find a valid IPv6 address!\n");
    exit(1);
  }

  printf("\n");

  // Now that we have an IP, create a socket
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    fprintf(stderr, "Could not create socket!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }
  printf("Socket created\n");

  // Connect to the remote over socket
  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    fprintf(stderr, "Could not connect to %s!\n", (char *)hostaddr);
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }
  printf("Connection established\n");

  // servinfo is no longer needed, dispose
  freeaddrinfo(res);

  // Send HTTP request
  char *message = "GET / HTTP/1.0\r\n\r\n";
  int len_tx = strlen(message);

  printf("Sending HTTP request '%s'...\n", message);
  if (send(sockfd, message, len_tx, 0) == -1) {
    fprintf(stderr, "Sending HTTP request failed!\n");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  // Await response (synchronously)
  // 256 KB, accomodates usual value of 212.9 KB set in
  // /proc/sys/net/core/rmem_max  int step_size = 256000;
  int step_size = 256000;
  int cursor = 0;
  int bytes_rx, total_bytes_rx = 0;
  int len_rx = step_size;
  void *buf = malloc(sizeof(char) * len_rx), *new_buf;

  printf("Awaiting response...\n");
  do {
    // Write bytes into the next free location in the buffer
    // Receive only as much as the amount of free space we have in the buffer
    bytes_rx = recv(sockfd, buf + cursor, len_rx - total_bytes_rx, 0);
    printf("received %d bytes\n", bytes_rx);

    // Keep count of the bytes received
    total_bytes_rx += bytes_rx;
    // Continue reading where we left off
    cursor += bytes_rx;

    // If the buffer is filled, increase its size
    if (total_bytes_rx == len_rx) {
      new_buf = realloc(buf, len_rx + step_size);

      // realloc may fail and return NULL
      if (new_buf != NULL) {
        buf = new_buf;       // Change original buffer to new buffer
        new_buf = NULL;      // Clear new_buf
        len_rx += step_size; // Increase buffer length
        printf("Extended buffer to length %d\n", len_rx);
      }
    }
  } while (bytes_rx > 0);

  // Close socket; we're done using it
  close(sockfd);

  // Error checking
  if (bytes_rx == -1) {
    fprintf(stderr, "Error receiving response!");
    fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  if (bytes_rx == 0)
    fprintf(stderr, "Remote has closed the connection\n");

  // Print message length
  printf("Message length: %d\n", total_bytes_rx);

  // Exit preemptively if response is empty
  if (total_bytes_rx == 0) {
    fprintf(stderr, "Empty response!");
    exit(1);
  }

  // Split file
  char *del = "\r\n\r\n";
  // Search for HTTP delimiter
  char *delimiter = strstr(buf, del);
  char *headers, *content;

  // If we couldn't find the delimiter, then the response is empty
  if (!delimiter) {
    fprintf(stderr, "Empty response!");
    exit(1);
  }

  // Set the headers to the buffer
  headers = buf;
  // "split" response into two by only printing the buffer up to \0
  *delimiter = '\0';
  // Only start content after the whole delimiter, until the end of the buffer
  // (or the first \0)
  content = buf + strlen(headers) + strlen(del);

  // Print headers
  printf("%s\n", headers);
  // Save content to `{host}.http`
  save_file(content, strlen(content), strcat(host, ".html"));

  return 0;
}
