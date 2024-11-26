#include "shared.h"
#include <stdarg.h>

bool DEBUG = false, DEBUG_SET = false;

// Check result of a command, and return its return value if it did not error
int check(int result, const char *message) {
  if (result == -1) {
    perrno(message);
    exit(1);
  }

  return result;
}

// Checks the return value of malloc, to ensure we don't try to use unallocated
// memory
void *malloc_s(size_t n) {
  void *p = malloc(n);

  if (p == NULL) {
    error("Failed to allocate %zu bytes", n);
    abort();
  }

  return p;
}

// Get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

char *recv_all(int sockfd, unsigned int num_bytes) {
  // Await response (synchronously)
  // 256 KB, accomodates usual value of 212.9 KB set in
  // /proc/sys/net/core/rmem_max  int step_size = 256000;
  int step_size = 256000;
  int cursor = 0;
  int bytes_rx = 0, total_bytes_rx = 0;
  int len_rx = step_size;
  void *buf = malloc_s(sizeof(char) * len_rx);
  void *new_buf;

  do {
    // We only want num_bytes, if non-zero
    if (num_bytes != 0 && total_bytes_rx >= num_bytes) {
      break;
    }

    // Write bytes into the next free location in the buffer
    // Receive only as much as the amount of free space we have in the buffer
    bytes_rx = recv(sockfd, buf + cursor, len_rx - total_bytes_rx, 0);
    debug("received %d bytes", bytes_rx);

    // Error checking
    if (check(bytes_rx, "Error receiving data") < 0) {
      free(buf);
      buf = NULL;
    }

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
        debug("Extended buffer to length %d", len_rx);
      } else {
        error("realloc failed!");
        free(buf);
        buf = NULL;
      }
    }
  } while (bytes_rx > 0);

  if (bytes_rx == 0)
    printf("Remote has closed the connection\n");

  if (num_bytes > 0 && total_bytes_rx > num_bytes) {
    void *newbuf = malloc_s(num_bytes);
    strncpy(newbuf, buf, num_bytes);
    free(buf);
    buf = newbuf;
  }

  return buf;
}

void send_all(int sockfd, char *buf, unsigned int num_bytes) {
  int bytes_tx = 0;
  int total_bytes_tx = 0;

  while (num_bytes - total_bytes_tx > 0) {
    bytes_tx =
        check(send(sockfd, buf + total_bytes_tx, num_bytes - total_bytes_tx, 0),
              "send failed");
    total_bytes_tx += bytes_tx;
    debug("sent %d bytes", bytes_tx);
  }
}

char **split_http_response(char *buf, long len) {
  char **container = malloc_s(2 * sizeof(char *));

  char *del = "\r\n\r\n";
  char *delimiter = strstr(buf, del);

  // If we couldn't find the delimiter, then the response is empty
  if (!delimiter) {
    error("Empty response!");
    free(container);
    exit(1);
  }

  // Allocate memory and copy headers
  size_t headers_len = delimiter - buf;
  container[0] = malloc_s(headers_len + 2);
  strncpy(container[0], buf, headers_len);
  // Newline, otherwise what we send leaks into the next request
  container[0][headers_len - 1] = '\n';
  // Null-terminate headers
  container[0][headers_len] = '\0';

  // Allocate memory and copy content
  size_t content_len = len - (headers_len + strlen(del));
  container[1] = malloc_s(content_len + 2);
  strncpy(container[1], delimiter + strlen(del), content_len);
  // Newline, otherwise what we send leaks into the next request
  container[0][content_len - 1] = '\n';
  // Null-terminate headers
  container[1][content_len] = '\0';

  // We're done using buf
  free(buf);

  return container;
}

void save_file(char *buffer, unsigned int length, char *file_name) {
  FILE *file = fopen(file_name, "w");

  if (file) {
    fwrite(buffer, sizeof(char), length, file);
  }

  debug("Saved file to %s\n", file_name);

  fclose(file);
}

void debug(const char *restrict format, ...) {
  if (!DEBUG_SET) {
    if (getenv("DEBUG") != NULL) {
      printf("Setting DEBUG to true\n");
      DEBUG = true;
    }

    DEBUG_SET = true;
  }

  if (!DEBUG) {
    return;
  }

  va_list vargs;
  // Begin orange colored output
  printf("\033[0;33m");
  va_start(vargs, format);
  vprintf(format, vargs);
  va_end(vargs);
  // End orange colored output
  printf("\033[0m\n");
}

void error(const char *restrict format, ...) {
  va_list vargs;
  fprintf(stderr, "ERROR: ");
  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  fprintf(stderr, "\n");
  va_end(vargs);
}

void perrno(const char *restrict format, ...) {
  va_list vargs;
  va_start(vargs, format);
  error(format, vargs);
  va_end(vargs);
  fprintf(stderr, "errno %d: %s\n", errno, strerror(errno));
}
