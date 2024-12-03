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

// Client that performs a HTTP 1.0 request to a pre-defined list of hostnames,
// selected through `cmd`. Valid cmds range from 0 to 21 (inclusive).
char *client(int cmd) {
  debug("Starting client...\n");

  // Define request
  const char *request = "GET / HTTP/1.0\r\n\r\n";

  // For debugging purposes, use IPv4 websites
  if (getenv("USE_IPV4")) {
    AF_FAMILY = AF_INET;
    IPV4 = true;
  }

  // Short-circuit for unknown commands
  if (cmd > DEST_MAX || cmd < 0) {
    return make_error_message("Command not implemented\n");
  }

  // 64 bytes should be enough
  char *host = malloc_s(64 * sizeof(char));
  int sockfd;

  strcpy(host, destinations[cmd]);

  struct addrinfo *res = get_ip_addrinfo(host, "http");

  // If no IP address was found, return error
  if (res == NULL) {
    char *error_resp = make_error_message(
        "Could not find IP%s address for %s!\n", IPV4 ? "v4" : "v6", host);
    free(host);

    perrno(error_resp);
    return error_resp;
  }

  // Print IP address of server
  char *addr_ip = get_ip_addrstr(res);

  // If no IP address was found, return error
  if (res == NULL) {
    char *error_resp = make_error_message(
        "Could not determine IP%s string representation for %s!\n",
        IPV4 ? "v4" : "v6", host);
    free(host);
    freeaddrinfo(res);

    return error_resp;
  }

  debug("IP%s address of %s: %s", IPV4 ? "v4" : "v6", host, addr_ip);
  free(addr_ip);

  // Now that we have an IP, create a socket
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) {
    free(host);
    freeaddrinfo(res);

    // Construct and return custom error message
    char *error_resp = make_error_message("Could not create socket!\n");
    perrno(error_resp);
    return error_resp;
  }
  debug("Socket created");

  // Set timeouts for the socket, because we don't want to wait forever
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  // Connect to the remote over socket
  int connect_resp = connect(sockfd, res->ai_addr, res->ai_addrlen);
  // servinfo is no longer needed, dispose
  freeaddrinfo(res);

  // If connect failed, error and return message
  if (connect_resp < 0) {
    char *error_resp = make_error_message("Could not connect to %s!\n", host);
    free(host);
    perrno(error_resp);

    return error_resp;
  };

  debug("Connection established");

  // Send HTTP request
  int len_tx = strlen(request);

  debug("Sending HTTP request '%s'...", request);
  check(send(sockfd, request, len_tx, 0), "Sending HTTP request failed!");

  // Receive response
  // 0 num_bytes because we don't expect a fixed response size
  long bytes_rx = 0;
  char *buf = recv_all(sockfd, 0);
  if (buf == NULL) {
    char *error_resp = make_error_message("No response\n");
    free(host);
    perrno(error_resp);
    return error_resp;
  }

  // Get number of received bytes
  bytes_rx = strlen(buf) + 1;
  debug("Message length: %ld", bytes_rx);

  // Close socket; we're done using it
  check(close(sockfd), "Could not close buffer");

  if (bytes_rx == 0) {
    // Construct and return custom error message
    char *error_resp = make_error_message("Received empty response\n");
    error(error_resp);
    return error_resp;
  }

  // Copy buf before passing buf to split_http_response, which frees it
  char *buf_copy = malloc_s(bytes_rx + 1);
  memcpy(buf_copy, buf, bytes_rx);
  // Ensure newline and NULL termination
  buf_copy[bytes_rx - 1] = '\n';
  buf_copy[bytes_rx] = '\0';

  // Split response into headers and content
  char **container = split_http_response(buf, bytes_rx);
  // If there was no HTML, early return
  if (container == NULL) {
    error("Could not save file because there was no HTML");
    free(host);
    return buf_copy;
  }

  char *headers = container[0];
  char *content = container[1];

  // Print headers
  printf("\n%s\n", headers);

  // Save content to `{host}.http`
  char *filename = malloc_s(strlen(host) + 6); // ".html\0"
  sprintf(filename, "%s.html", host);

  save_file(content, strlen(content), filename);

  // We're done with host, the file, and the container
  free(host);
  free(filename);
  free(headers);
  free(content);
  free(container);

  // Return the temporary pointer and let the caller handle it
  return buf_copy;
}

// Get the addrinfo for a given hostname and service name
struct addrinfo *get_ip_addrinfo(const char *name, const char *service) {
  struct addrinfo hints, *res;
  int status;

  memset(&hints, 0, sizeof hints); // zero-init the struct
  hints.ai_family = AF_FAMILY;     // we only want IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = 0;

  // This may leak due to a bug in glibc and the way the DNS resolver is called,
  // but is not an error
  if ((status = getaddrinfo(name, "http", &hints, &res)) != 0) {
    error("getaddrinfo error: %s\n", gai_strerror(status));
    freeaddrinfo(res);
    return NULL;
  }

  return res;
}

// Get the string representation of an IP address returned by `get_ip_addrinfo`
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
    return NULL;
  }

  return hostaddr;
}
