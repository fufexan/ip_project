#include "netinet/in.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int main() {
  printf("Starting IPv6 client...\n");

  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;       // points to the results
  memset(&hints, 0, sizeof hints); // zero-init the struct
  hints.ai_family = AF_INET6;      // we only want IPv6 this time
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // autofill IP

  if ((status = getaddrinfo("google.com", "http", &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  }

  char ipstr[INET6_ADDRSTRLEN];
  struct addrinfo *p;

  // Check all results
  for (p = servinfo; p != NULL; p = p->ai_next) {
    void *addr;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    addr = &(ipv6->sin6_addr);

    // convert the IP to a string and print it:
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    printf(" %s\n", ipstr);
  }

  freeaddrinfo(servinfo);
}
