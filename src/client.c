#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "./destinations.h"
#include "./shared.h"

struct addrinfo *get_ipv6_addrinfo(const char *name, const char *service) {
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

char *get_ipv6_addrstr(struct addrinfo *res) {
  struct addrinfo *p; // iterator
  char *hostaddr = malloc(INET6_ADDRSTRLEN);
  bool addr_found = false;

  // Check all results, stop on first valid address
  for (p = res; p != NULL && !addr_found; p = p->ai_next) {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    struct in6_addr *addr = &(ipv6->sin6_addr);

    // convert the IP to a string and print it
    const char *ptr = inet_ntop(AF_INET6, addr, hostaddr, INET6_ADDRSTRLEN);

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

  return hostaddr;
}


int main(int argc, char **argv) {
  printf("Starting IPv6 client...\n");

  // 64 bytes should be enough
  char *host = calloc(64, sizeof(char));
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

      printf("Using host %s", host);
    }
  }

  struct addrinfo *res = get_ipv6_addrinfo(host, "http");
  printf("IPv6 address of %s: %s\n", host, get_ipv6_addrstr(res));

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
    fprintf(stderr, "Could not connect to %s!\n", host);
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

  // Receive response
  // 0 num_bytes because we don't expect a fixed response size
  char *buf = receive(sockfd, 0);
  long total_bytes_rx = strlen(buf);

  // Close socket; we're done using it
  close(sockfd);

  // Print message length
  printf("Message length: %ld\n", total_bytes_rx);

  // Exit preemptively if response is empty
  if (total_bytes_rx == 0) {
    fprintf(stderr, "Empty response!");
    exit(1);
  }

  char **container = split_http_response(buf, total_bytes_rx);
  char *headers = container[0];
  char *content = container[1];

  // Print headers
  printf("%s\n", headers);
  // Save content to `{host}.http`
  save_file(content, strlen(content), strcat(host, ".html"));

  return 0;
}
