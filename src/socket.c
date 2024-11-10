#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "5556"
#define PENDING_CONNECTIONS 20

int get_socket() {
  int addrinfo_status = 0;
  struct addrinfo hints;
  struct addrinfo *server_info;
  struct addrinfo *server_to_bind_info;

  // get addr info
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // will end up binding localhost

  if ((addrinfo_status = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(addrinfo_status));
    exit(1);
  }

  // get socket
  int socket_descriptor;
  for (server_to_bind_info = server_info; server_to_bind_info;
       server_to_bind_info = server_to_bind_info->ai_next) {

    socket_descriptor = socket(server_info->ai_family, server_info->ai_socktype,
                               server_info->ai_protocol);
    if (socket_descriptor == -1) {
      perror("server getting socket");
      continue;
    }

    int opt = 1;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(socket_descriptor, server_to_bind_info->ai_addr,
             server_to_bind_info->ai_addrlen) == -1) {
      close(socket_descriptor);
      perror("binding server");
      continue;
    }
    break;
  }

  if (server_to_bind_info == NULL) {
    fprintf(stderr, "failed to bind socket for server\n");
    exit(1);
  }

  freeaddrinfo(server_info);

  // listen
  if (listen(socket_descriptor, PENDING_CONNECTIONS) == -1) {
    perror("listen");
    exit(1);
  }

  return socket_descriptor;
}

int accept_connection(int socket_descriptor) {
  struct sockaddr_storage accepted_sockaddr;
  socklen_t accepted_addr_size = sizeof(accepted_sockaddr);
  int accepted_socket =
      accept(socket_descriptor, (struct sockaddr *)&accepted_sockaddr,
             &accepted_addr_size);

  return accepted_socket;
}
