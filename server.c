#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

int new_socket(struct sockaddr_in *addr_ptr, unsigned short port) {
  int sock = socket(DOMAIN, SOCK_DGRAM, 0);
  checkerr(sock, "socket");

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct timeval tv;
  tv.tv_sec = 30;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  memset((char *)addr_ptr, 0, sizeof(struct sockaddr_in));
  addr_ptr->sin_family = DOMAIN;
  addr_ptr->sin_port = htons(port);
  addr_ptr->sin_addr.s_addr = htonl(INADDR_ANY);

  int err = bind(sock, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(err, "bind");

  printPID();
  printf("New UDP socket %d listening on port %d\n", sock, port);

  return sock;
}

void handle_client(int c_sock, struct sockaddr_in *c_addr_ptr) {
  char msg_req[MSG_LENGTH];
  my_recv_str(c_sock, msg_req, c_addr_ptr);

  if (strncmp(msg_req, GET, strlen(GET)) == 0) {
    char *filename = &msg_req[strlen(GET) + 1];

    // read file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
      printf("Error opening file %s\n", filename);
      exit(1);
    }

    struct file_segment_with_no seg;
    seg.no = 1;

    // send file chunk by chunk
    size_t bytes_read;
    while ((bytes_read = fread(seg.data, 1, FILE_CHUNK_SIZE, fp)) > 0) {
      seg.size = bytes_read;

      char ack_with_no[sizeof(ACK_NO) + ACK_NO_LENGTH + 1];
      sprintf(ack_with_no, "%s%d", ACK_NO, seg.no);

      do {
        my_send_bytes(c_sock, (char *)&seg, sizeof(struct file_segment_with_no), c_addr_ptr);
      } while (!recv_control_str(c_sock, ack_with_no, c_addr_ptr));

      seg.no++;
    }

    fclose(fp);

    my_send_str(c_sock, FIN, c_addr_ptr);
  } else {
    printf("Unknown request %s\n", msg_req);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <udp port>\n", argv[0]);
    exit(1);
  }
  unsigned short server_port = atoi(argv[1]);
  struct sockaddr_in server_addr;
  int server_socket = new_socket(&server_addr, server_port);

  unsigned short client_no = 1;

  while (1) {
    struct sockaddr_in c_addr;

    if (recv_control_str(server_socket, SYN, &c_addr)) {
      // SYN-ACK <port>
      char syn_ack[sizeof(SYN_ACK) + PORT_LENGTH + 1];
      short c_port = server_port + client_no++;
      sprintf(syn_ack, "%s%d", SYN_ACK, c_port);

      // create new socket for client
      struct sockaddr_in c_server_addr;
      int c_sock = new_socket(&c_server_addr, c_port);

      // send SYN-ACK
      do {
        my_send_str(server_socket, syn_ack, &c_addr);
      } while (!recv_control_str(server_socket, ACK, &c_addr));

      if (fork() == 0) {
        close(server_socket);
        handle_client(c_sock, &c_addr);
        close(c_sock);
        exit(0);
      } else {
        close(c_sock);
      }
    }
  }

  return 0;
}
