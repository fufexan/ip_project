#include "netinet/in.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int main() {
  printf("Starting IPv6 client...\n");

  char host[] = "google.com";
  void *hostip; // char[INET6_ADDRSTRLEN]
  bool addr_found = false;
  int sockfd;

  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;       // points to the results
  memset(&hints, 0, sizeof hints); // zero-init the struct
  hints.ai_family = AF_INET6;      // we only want IPv6 this time
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // autofill IP

  if ((status = getaddrinfo(host, "http", &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  }

  char ipstr[INET6_ADDRSTRLEN];
  struct addrinfo *p;

  printf("IPv6 addresses of %s:\n\n", host);

  // Check all results
  for (p = servinfo; p != NULL && !addr_found; p = p->ai_next) {
    void *addr;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    addr = &(ipv6->sin6_addr);

    // convert the IP to a string and print it:
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    printf(" %s\n", ipstr);

    // TODO: more robust check
    hostip = &ipstr;
    addr_found = true;
  }

  if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                       servinfo->ai_protocol)) == -1) {
    fprintf(stderr, "Could not create socket!\n");
    return 1;
  }

  if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    fprintf(stderr, "Could not connect to %s: errno %d\n%s\n", (char *)hostip,
            errno, strerror(errno));
    return 1;
  }

  char *msg = "GET / HTTP/1.0\r\n\r\n";
  int len, bytes_sent;
  len = strlen(msg);
  bytes_sent = send(sockfd, msg, len, 0);

  if (bytes_sent == -1) {
    fprintf(stderr, "errno %d\n%s\n", errno, strerror(errno));
    return 1;
  }

  len = 256 * 1000; // 256 KB
  void *buf = calloc(len, sizeof(char));
  int bytes_received;
  bytes_received = recv(sockfd, buf, len, 0);

  if (bytes_received == -1) {
    fprintf(stderr, "errno %d\n%s\n", errno, strerror(errno));
    return 1;
  }
  if (bytes_received == 0) {
    fprintf(stderr, "Remote has closed the connection\n");
    return 0;
  }
  printf("%s", (char *)buf);

  freeaddrinfo(servinfo);
}
