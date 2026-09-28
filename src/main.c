#include "database.h"
#include "network.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 5000

static volatile sig_atomic_t running = 1;

static void handle_signal(int signal)
{
  (void)signal;
  running = 0;
}

static void serve_client(sqlite3* db, int fd)
{
  while (running)
  {
    char* payload = NULL;
    size_t length = 0;

    if (!network_receive_frame(fd, &payload, &length, &running))
    {
      break;
    }

    ProtocolMessage message;
    const char* error = protocol_parse(payload, length, &message);
    free(payload);

    if (!error)
    {
      error = database_save(db, &message);
    }

    const char* id = message.message_id ? message.message_id : "";

    if (error)
    {
      fprintf(stderr, "Message %s: %s\n", id, error);
    }

    char* ack = protocol_make_ack(id, error);
    int result = ack ? network_send_frame(fd, ack, strlen(ack), &running) : -1;
    free(ack);
    protocol_free(&message);

    if (result < 0)
    {
      if (running)
      {
        perror("send ack");
      }

      break;
    }
  }
}

int main(void)
{
  struct sigaction action = {0};
  action.sa_handler = handle_signal;
  sigemptyset(&action.sa_mask);

  if (sigaction(SIGINT, &action, NULL) < 0 ||
      sigaction(SIGTERM, &action, NULL) < 0)
  {
    perror("sigaction");
    return 1;
  }

  sqlite3* db = database_open();

  if (!db)
  {
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd < 0)
  {
    perror("socket");
    database_close(db);
    return 1;
  }

  int reuse = 1;

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    perror("setsockopt");
    close(server_fd);
    database_close(db);
    return 1;
  }

  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
  {
    perror("bind");
    close(server_fd);
    database_close(db);
    return 1;
  }

  if (listen(server_fd, 8) < 0)
  {
    perror("listen");
    close(server_fd);
    database_close(db);
    return 1;
  }

  printf("Edge Relay listening on 0.0.0.0:%d\n", PORT);
  fflush(stdout);

  while (running)
  {
    struct sockaddr_in client = {0};
    socklen_t client_length = sizeof(client);
    int fd = accept(server_fd, (struct sockaddr*)&client, &client_length);

    if (fd < 0)
    {
      if (errno == EINTR && !running)
      {
        break;
      }

      perror("accept");
      continue;
    }

    if (network_set_timeout(fd) < 0)
    {
      perror("client timeout");
      close(fd);
      continue;
    }

    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
    printf("Client connected: %s:%d\n", ip, ntohs(client.sin_port));
    serve_client(db, fd);
    close(fd);
  }

  close(server_fd);
  database_close(db);
  printf("Edge Relay stopped\n");
  return 0;
}
