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

// Checks the return value of malloc, to ensure we don't try to use unallocated
// memory
void *realloc_s(void *buf, size_t n) {
  void *p = realloc(buf, n);

  if (p == NULL) {
    error("Failed to reallocate %zu bytes", n);
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
  // Use a small step size to demonstrate resilient behavior
  int step_size = 10;
  int cursor = 0, bytes_rx = 0, total_bytes_rx = 0;
  int len_rx = step_size;
  char *buf = malloc_s(sizeof(char) * len_rx);

  do {
    // We only want num_bytes, if non-zero
    if (num_bytes != 0 && total_bytes_rx >= num_bytes) {
      break;
    }

    // Write bytes into the next free location in the buffer
    // Receive only as much as the amount of free space we have in the buffer
    bytes_rx = check(recv(sockfd, buf + cursor, len_rx - total_bytes_rx, 0),
                     "recv failed");
    debug("received %d bytes", bytes_rx);

    if (bytes_rx == 0) {
      debug("Remote has closed the connection");
      break;
    }

    // Keep count of the bytes received
    total_bytes_rx += bytes_rx;
    // Continue reading where we left off
    cursor += bytes_rx;

    // If the buffer is filled, increase its size
    if (total_bytes_rx == len_rx) {
      buf = realloc_s(buf, len_rx + step_size);
      len_rx += step_size;
      debug("Extended buffer to length %d", len_rx);
    }
  } while (bytes_rx > 0);

  if (bytes_rx == 0)
    printf("Remote has closed the connection\n");

  // Truncate if num_bytes is specified
  if (num_bytes > 0 && total_bytes_rx > num_bytes) {
    char *truncated_buf = malloc_s(num_bytes + 1);
    memcpy(truncated_buf, buf, num_bytes);
    truncated_buf[num_bytes] = '\0'; // Null-terminate if needed
    free(buf);
    buf = truncated_buf;
    total_bytes_rx = num_bytes;
  }

  // Ensure null-termination
  if (total_bytes_rx < len_rx) {
    buf[total_bytes_rx] = '\0';
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
  container[1][content_len - 1] = '\n';
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
