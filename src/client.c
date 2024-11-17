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
    error("getaddrinfo error: %s\n", gai_strerror(status));
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
      perrno("Failed to get string representation of IPv6 address!");
    } else {
      debug("%s", hostaddr);
      addr_found = true;
    }
  }

  if (!addr_found) {
    error("Could not find a valid IPv6 address!");
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
      error("Invalid hostname! Please pick a number between 0 and %d",
            DEST_MAX);

      printf("Using host %s\n", host);
    }
  }

  struct addrinfo *res = get_ipv6_addrinfo(host, "http");
  debug("IPv6 address of %s: %s", host, get_ipv6_addrstr(res));

  // Now that we have an IP, create a socket
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perrno("Could not create socket!");
    exit(1);
  }
  debug("Socket created");

  // Connect to the remote over socket
  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    perrno("Could not connect to %s!", host);
    exit(1);
  }
  debug("Connection established");

  // servinfo is no longer needed, dispose
  freeaddrinfo(res);

  // Send HTTP request
  char *message = "GET / HTTP/1.0\r\n\r\n";
  int len_tx = strlen(message);

  debug("Sending HTTP request '%s'...", message);
  if (send(sockfd, message, len_tx, 0) == -1) {
    perrno("Sending HTTP request failed!");
    exit(1);
  }

  // Receive response
  // 0 num_bytes because we don't expect a fixed response size
  char *buf = receive(sockfd, 0);
  long total_bytes_rx = strlen(buf);

  // Close socket; we're done using it
  close(sockfd);

  // Print message length
  debug("Message length: %ld", total_bytes_rx);

  // Exit preemptively if response is empty
  if (total_bytes_rx == 0) {
    error("Empty response!");
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
