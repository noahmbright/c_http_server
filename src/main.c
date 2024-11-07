#include "http_server.h"
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_LENGTH (4096)

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

int main() {

  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  int socket = get_socket();
  char buffer[BUFFER_LENGTH];

  while (1) {
    int accepted_socket = accept_connection(socket);

    unsigned received_bytes = 0;
    if ((received_bytes = recv(accepted_socket, buffer, BUFFER_LENGTH, 0)) ==
        -1) {
      perror("received -1 bytes");
      exit(1);
    }
    buffer[received_bytes] = '\0';

    printf("%s\n", buffer);

    if (!fork()) {
      close(socket);

      unsigned sent_bytes;
      if ((sent_bytes = send(accepted_socket, "HTTP/1.0 200", 12, 0)) == -1) {
        perror("sent -1 bytes");
        exit(1);
      }
      close(accepted_socket);
      exit(0);
    }

    close(accepted_socket);
  }

  return 0;
}