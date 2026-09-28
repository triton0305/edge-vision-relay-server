#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>

#define MAX_PAYLOAD 1048576U

static ssize_t read_all(int fd, void* buffer, size_t length,
                        volatile sig_atomic_t* running)
{
  size_t total = 0;

  while (total < length)
  {
    ssize_t n = recv(fd, (char*)buffer + total, length - total, 0);

    if (n < 0)
    {
      if (errno == EINTR && *running)
      {
        continue;
      }

      return -1;
    }

    if (n == 0)
    {
      break;
    }

    total += (size_t)n;
  }

  return (ssize_t)total;
}

static int write_all(int fd, const void* buffer, size_t length,
                     volatile sig_atomic_t* running)
{
  size_t total = 0;

  while (total < length)
  {
    ssize_t n = send(fd, (const char*)buffer + total, length - total, MSG_NOSIGNAL);

    if (n < 0)
    {
      if (errno == EINTR && *running)
      {
        continue;
      }

      return -1;
    }

    if (n == 0)
    {
      return -1;
    }

    total += (size_t)n;
  }

  return 0;
}

int network_set_timeout(int fd)
{
  struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
  {
    return -1;
  }

  return 0;
}

int network_receive_frame(int fd, char** payload, size_t* length,
                          volatile sig_atomic_t* running)
{
  uint32_t prefix = 0;
  ssize_t n = read_all(fd, &prefix, sizeof(prefix), running);

  if (n <= 0)
  {
    if (n < 0 && *running)
    {
      perror("read length");
    }

    return 0;
  }

  if (n != sizeof(prefix))
  {
    fprintf(stderr, "Incomplete length prefix: %zd bytes\n", n);
    return 0;
  }

  uint32_t payload_length = ntohl(prefix);

  if (!payload_length || payload_length > MAX_PAYLOAD)
  {
    fprintf(stderr, "Invalid payload length: %u\n", payload_length);
    return 0;
  }

  char* data = malloc((size_t)payload_length + 1);

  if (!data)
  {
    fprintf(stderr, "Payload allocation failed\n");
    return 0;
  }

  n = read_all(fd, data, payload_length, running);

  if (n != (ssize_t)payload_length)
  {
    if (n < 0 && *running)
    {
      perror("read payload");
    }
    else if (n >= 0)
    {
      fprintf(stderr, "Incomplete payload: %zd/%u bytes\n", n, payload_length);
    }

    free(data);
    return 0;
  }

  data[payload_length] = '\0';
  *payload = data;
  *length = payload_length;
  return 1;
}

int network_send_frame(int fd, const char* payload, size_t length,
                       volatile sig_atomic_t* running)
{
  uint32_t prefix = htonl((uint32_t)length);
  int result = write_all(fd, &prefix, sizeof(prefix), running);

  if (result == 0)
  {
    result = write_all(fd, payload, length, running);
  }

  return result;
}
