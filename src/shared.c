#include "shared.h"
#include <stdarg.h>
#include <sysexits.h>

bool DEBUG = false, DEBUG_SET = false;

// Check result of a command, and return its return value if it did not error.
// Works for commands that return values less than 0 on error.
int check(int result, const char *message) {
  if (result < 0) {
    perrno(message);
  }

  return result;
}

// Checks the return value of `malloc`, to ensure we don't try to use
// unallocated memory
void *malloc_s(size_t n) {
  void *p = malloc(n);

  if (p == NULL) {
    error("Failed to allocate %zu bytes", n);
    abort();
  }

  return p;
}

// Checks the return value of `realloc`, to ensure we don't try to use
// unallocated memory
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

// Receive the entire message on `sockfd`, until the number of received bytes is
// 0. Optionally limit the number of bytes to be received by setting `num_bytes`
// to a non-zero value.
char *recv_all(int sockfd, unsigned int num_bytes) {
  // Await response (synchronously)
  // Start with a small buffer size to demonstrate resilient behavior
  size_t buf_len = 2, cursor = 0;
  int bytes_rx = 0, total_rx = 0;

  char *buf = malloc_s(sizeof(char) * buf_len);

  do {
    // We only want num_bytes, if non-zero
    if (num_bytes != 0 && total_rx >= num_bytes) {
      break;
    }

    // Write bytes into the next free location in the buffer
    // Receive only as much as the amount of free space we have in the buffer
    bytes_rx = recv(sockfd, buf + cursor, buf_len - total_rx, 0);

    // Early return if we got an error
    if (bytes_rx < 0) {
      perrno("Received 0 bytes when calling recv");
      free(buf);
      return NULL;
    }

    debug("received %d bytes", bytes_rx);

    // When we receive 0 bytes, the server has closed the connection
    if (bytes_rx == 0) {
      break;
    }

    // Keep count of the bytes received
    total_rx += bytes_rx;
    // Continue reading where we left off
    cursor += bytes_rx;

    // If the buffer is full, double its size
    if (total_rx == buf_len) {
      buf = realloc_s(buf, buf_len * 2);
      buf_len *= 2;
      debug("Extended buffer to size %d", buf_len);
    }
  } while (bytes_rx > 0);

  if (bytes_rx == 0)
    printf("Remote has closed the connection on fd %d\n", sockfd);

  // Truncate if num_bytes is specified
  if (num_bytes > 0 && total_rx > num_bytes) {
    char *truncated_buf = malloc_s(num_bytes + 1);
    memcpy(truncated_buf, buf, num_bytes);

    // Ensure null-termination
    truncated_buf[num_bytes] = '\0';

    free(buf);
    buf = truncated_buf;
    total_rx = num_bytes;
  }

  // Ensure null-termination
  if (total_rx < buf_len) {
    buf[total_rx] = '\0';
  }

  return buf;
}

// Send an entire message by repeatedly calling `send` as needed
void send_all(int sockfd, char *buf, unsigned int num_bytes) {
  int bytes_tx = 0;
  int total_tx = 0;

  while (num_bytes - total_tx > 0) {
    bytes_tx = send(sockfd, buf + total_tx, num_bytes - total_tx, 0);

    // Log error and early return
    if (bytes_tx < 0) {
      perrno("recv error");
      return;
    }

    total_tx += bytes_tx;
    debug("sent %d bytes", bytes_tx);
  }
}

// Split a buffer into two, `headers` (`container[0]`) and `content`
// (`container[1]`), based on a HTTP ending (`\r\n\r\n`).
//
// `buf` is consumed and freed in the process, and it is up to the calling code
// to free the resulting `char **` (`container`).
//
// If the response is empty, return NULL
char **split_http_response(char *buf, long len) {
  char **container = malloc_s(2 * sizeof(char *));

  char *del = "\r\n\r\n";
  char *delimiter = strstr(buf, del);

  // If we couldn't find the delimiter, then the response is empty
  if (!delimiter) {
    error("Empty response!");
    free(container);
    free(buf);
    return NULL;
  }

  // Allocate memory and copy headers
  size_t headers_len = delimiter - buf;
  container[0] = malloc_s(headers_len + 2);
  memcpy(container[0], buf, headers_len);
  // Newline, otherwise what we send leaks into the next request
  container[0][headers_len] = '\n';
  // Null-terminate headers
  container[0][headers_len + 1] = '\0';

  // Allocate memory and copy content
  size_t content_len = len - (headers_len + strlen(del));
  container[1] = malloc_s(content_len + 2);
  memcpy(container[1], delimiter + strlen(del), content_len);
  // Newline, otherwise what we send leaks into the next request
  container[1][content_len] = '\n';
  // Null-terminate headers
  container[1][content_len + 1] = '\0';

  // We're done using buf
  free(buf);

  return container;
}

// Write `buffer` to a file called `file_name`.
void save_file(char *buffer, unsigned int length, char *file_name) {
  FILE *file = fopen(file_name, "w");

  if (!file) {
    perrno("Could not open file for writing");
  }

  size_t bytes_written = fwrite(buffer, sizeof(char), length, file);

  if (bytes_written != length) {
    perrno("File writing");
  }

  debug("Saved file to %s\n", file_name);

  if (fclose(file) == EOF) {
    perrno("Could not close and flush file");
  }
}

// Helper print functions
// All of them exit the program with the exit code EX_IOERR (74) in case there
// was an output error

// Pretty print non-critical messages
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

// Print error messages to stderr with the prefix `ERROR: `
void error(const char *restrict format, ...) {
  va_list vargs;

  fprintf(stderr, "ERROR: ");

  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  va_end(vargs);

  fprintf(stderr, "\n");
}

// Print the desired message followed by the errno and its explanation on a
// newline
void perrno(const char *restrict format, ...) {
  va_list vargs;

  fprintf(stderr, "ERROR: ");

  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  va_end(vargs);

  fprintf(stderr, "\nerrno %d: %s\n", errno, strerror(errno));
}
