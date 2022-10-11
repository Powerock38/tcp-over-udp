#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

int new_socket(struct sockaddr_in *addr_ptr, unsigned short port) {
  int sock = socket(DOMAIN, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("socket returns %d\n", sock);
    return -1;
  }

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  memset((char *)addr_ptr, 0, sizeof(struct sockaddr_in));
  addr_ptr->sin_family = DOMAIN;
  addr_ptr->sin_port = htons(port);
  addr_ptr->sin_addr.s_addr = htonl(INADDR_ANY);

  int err = bind(sock, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  if (err < 0) {
    printf("udp bind returns %d\n", err);
    return -1;
  }

  printf("New UDP socket %d listening on port %d\n", sock, port);

  return sock;
}

int main(int argc, char *argv[]) {
  long err;

  if (argc != 2) {
    printf("Usage: %s <udp port>\n", argv[0]);
    return 1;
  }
  unsigned short server_port = atoi(argv[1]);
  struct sockaddr_in server_addr;
  int server_socket = new_socket(&server_addr, server_port);
  if (server_socket < 0) {
    return 1;
  }

  unsigned short client_no = 1;

  while (1) {
    struct sockaddr_in remote_addr;

    if (recv_control_str(server_socket, SYN, &remote_addr)) {
      // SYN-ACK <port>
      char syn_ack[sizeof(SYN_ACK) + PORT_LENGTH + 1];
      short c_port = server_port + client_no++;
      sprintf(syn_ack, "%s%d", SYN_ACK, c_port);

      // create new socket for client
      struct sockaddr_in c_server_addr;
      int c_sock = new_socket(&c_server_addr, c_port);
      if (c_sock < 0) {
        return 1;
      }

      // send SYN-ACK
      err = my_send_str(server_socket, syn_ack, &remote_addr);
      if (err < 0) {
        printf("my_send_str returns %ld\n", err);
        return 1;
      }

      if (recv_control_str(server_socket, ACK, &remote_addr)) {
        // todo fork: close(server_socket);

        char msg_req[MSG_LENGTH];

        err = my_recv_str(c_sock, msg_req, &remote_addr);
        if (err < 0) {
          printf("my_recv_str returns %ld\n", err);
          return 1;
        }

        if (strncmp(msg_req, GET, strlen(GET)) == 0) {
          char *filename = msg_req + strlen(GET) + 1;

          // read file
          FILE *fp = fopen(filename, "r");
          if (fp == NULL) {
            printf("Error opening file %s\n", filename);
            return 1;
          }

          struct file_segment_with_no seg;
          seg.no = 1;

          // send file chunk by chunk
          size_t bytes_read = 0;
          while ((bytes_read = fread(seg.data, 1, FILE_CHUNK_SIZE, fp)) > 0) {
            seg.size = bytes_read;
            err = my_send_bytes(c_sock, (char *)&seg, sizeof(struct file_segment_with_no), &remote_addr);
            if (err < 0) {
              printf("my_send_bytes returns %ld\n", err);
              return 1;
            }

            char ack_with_no[sizeof(ACK_NO) + ACK_NO_LENGTH + 1];
            sprintf(ack_with_no, "%s%d", ACK_NO, seg.no);

            if (recv_control_str(c_sock, ack_with_no, &remote_addr)) {
              seg.no++;
            } else {
              return 1;
            }
          }

          fclose(fp);

          err = my_send_str(c_sock, FIN, &remote_addr);
          if (err < 0) {
            printf("my_send_str returns %ld\n", err);
            return 1;
          }

        } else {
          printf("Unknown request %s\n", msg_req);
        }

      } else {
        printf("ACK not received, connection failed\n");
      }
    }
  }

  return 0;
}
