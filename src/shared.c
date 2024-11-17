#include "shared.h"
#include <stdarg.h>

bool DEBUG = false, DEBUG_SET = false;

char *receive(int sockfd, unsigned int num_bytes) {
  // Await response (synchronously)
  // 256 KB, accomodates usual value of 212.9 KB set in
  // /proc/sys/net/core/rmem_max  int step_size = 256000;
  int step_size = 256000;
  int cursor = 0;
  int bytes_rx, total_bytes_rx = 0;
  int len_rx = step_size;
  void *buf = malloc(sizeof(char) * len_rx), *new_buf;

  do {
    // We only want num_bytes, if non-zero
    if (num_bytes != 0 && total_bytes_rx >= num_bytes) {
      break;
    }

    // Write bytes into the next free location in the buffer
    // Receive only as much as the amount of free space we have in the buffer
    bytes_rx = recv(sockfd, buf + cursor, len_rx - total_bytes_rx, 0);
    debug("received %d bytes", bytes_rx);

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
      }
    }
  } while (bytes_rx > 0);

  // Error checking
  if (bytes_rx == -1) {
    error("Error receiving data!");
    error("errno %d: %s", errno, strerror(errno));
    exit(1);
  }

  if (bytes_rx == 0)
    printf("Remote has closed the connection\n");

  return buf;
}

char **split_http_response(char *buf, long len) {
  char **container = malloc(2 * sizeof(char *));

  char *del = "\r\n\r\n";
  char *delimiter = strstr(buf, del);
  char *headers, *content;

  // If we couldn't find the delimiter, then the response is empty
  if (!delimiter) {
    error("Empty response!");
    exit(1);
  }

  // Set the headers to the buffer
  headers = buf;
  // "split" response into two by only printing the buffer up to \0
  *delimiter = '\0';
  // Only start content after the whole delimiter, until the end of the buffer
  // (or the first \0)
  content = buf + strlen(headers) + strlen(del);

  container[0] = headers;
  container[1] = content;
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
