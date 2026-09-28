#ifndef NETWORK_H
#define NETWORK_H

#include <signal.h>
#include <stddef.h>

int network_set_timeout(int fd);
int network_receive_frame(int fd, char** payload, size_t* length,
                          volatile sig_atomic_t* running);
int network_send_frame(int fd, const char* payload, size_t length,
                       volatile sig_atomic_t* running);

#endif
