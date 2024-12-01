#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "destinations.h"
#include "shared.h"

int AF_FAMILY = AF_INET6;
bool IPV4 = false;

struct addrinfo *get_ip_addrinfo(const char *name, const char *service) {
  struct addrinfo hints, *res;
  int status;

  memset(&hints, 0, sizeof hints); // zero-init the struct
  hints.ai_family = AF_FAMILY;     // we only want IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = 0;

  if ((status = getaddrinfo(name, "http", &hints, &res)) != 0) {
    error("getaddrinfo error: %s\n", gai_strerror(status));
  }

  return res;
}

char *get_ip_addrstr(struct addrinfo *res) {
  int hostlen = IPV4 ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
  char *hostaddr = malloc_s(hostlen * sizeof(char));
  bool addr_found = false;

  // Check all results, stop on first valid address
  for (struct addrinfo *p = res; p != NULL && !addr_found; p = p->ai_next) {
    // convert the IP to a string and print it
    debug("af_family: %d", AF_FAMILY);
    void *in_addr = get_in_addr(p->ai_addr);
    const char *ntop = inet_ntop(AF_FAMILY, in_addr, hostaddr, hostlen);

    if (!ntop) {
      perror("Failed to get string representation of remote address");
    } else {
      debug("%s", ntop);
      addr_found = true;
    }
  }

  if (!addr_found) {
    error("Could not find a valid IP%s address!", IPV4 ? "v4" : "v6");
    free(hostaddr);
    exit(1);
  }

  return hostaddr;
}

char *client(int cmd) {
  debug("Starting client...\n");

  // For debugging purposes, use IPv4 websites
  char *inet_family = getenv("USE_IPV4");
  if (inet_family) {
    AF_FAMILY = AF_INET;
    IPV4 = true;
  }

  // Short-circuit for unknown commands
  if (cmd > DEST_MAX || cmd < 0) {
    return "Command not implemented\n";
  }

  // 64 bytes should be enough
  char *host = malloc_s(64 * sizeof(char));
  int sockfd;

  strcpy(host, destinations[cmd]);

  struct addrinfo *res = get_ip_addrinfo(host, "http");
  char *addr_ip = get_ip_addrstr(res);
  debug("IP%s address of %s: %s", IPV4 ? "v4" : "v6", host, addr_ip);
  free(addr_ip);

  // Now that we have an IP, create a socket
  sockfd = check(socket(res->ai_family, res->ai_socktype, res->ai_protocol),
                 "Could not create socket!");
  debug("Socket created");

  // Connect to the remote over socket
  char *str = malloc_s(sizeof(char) * (64 + INET6_ADDRSTRLEN));
  sprintf(str, "Could not connect to %s!", host);
  if(connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
    error(str);
    freeaddrinfo(res);
    free(str);
    exit(1);
  };
  free(str);

  debug("Connection established");

  // servinfo is no longer needed, dispose
  freeaddrinfo(res);

  // Send HTTP request
  char *message = "GET / HTTP/1.0\r\n\r\n";
  int len_tx = strlen(message);

  debug("Sending HTTP request '%s'...", message);
  check(send(sockfd, message, len_tx, 0), "Sending HTTP request failed!");

  // Receive response
  // 0 num_bytes because we don't expect a fixed response size
  long total_bytes_rx = 0;
  char *buf = recv_all(sockfd, 0);
  if (buf != NULL) {
    total_bytes_rx = strlen(buf);
  }

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
  char *filename = malloc_s(strlen(host) + 6); // ".html\0"
  sprintf(filename, "%s.html", host);

  save_file(content, strlen(content), filename);

  // We're done with host
  free(host);
  free(filename);

  // Create temporary pointer and copy data in memory
  char *html = malloc_s(strlen(content) + 1);
  strcpy(html, content);

  // Free container
  free(headers);
  free(content);
  free(container);

  // Return the temporary pointer and let the caller handle it
  return html;
}
