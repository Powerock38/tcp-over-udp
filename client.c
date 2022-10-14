#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <server ip> <server udp port>\n", argv[0]);
    exit(1);
  }
  char *server_ip = argv[1];
  short server_port = atoi(argv[2]);

  int sock = socket(DOMAIN, SOCK_DGRAM, 0);
  checkerr(sock, "socket");

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in addr;
  long err = inet_aton(server_ip, &(addr.sin_addr));
  checkerr(err, "inet_aton");

  addr.sin_port = htons(server_port);
  addr.sin_family = DOMAIN;

  printf("UDP client connecting to %s:%d\n", server_ip, server_port);

  send_str(sock, SYN, &addr);

  char msg[MSG_LENGTH];
  recv_str(sock, msg, &addr);

  if (strncmp(msg, SYN_ACK, strlen(SYN_ACK)) == 0) {
    char str_port[PORT_LENGTH];
    strncpy(str_port, &msg[strlen(SYN_ACK)], PORT_LENGTH);
    unsigned short c_port = atoi(str_port);

    send_str(sock, ACK, &addr);

    // at this point, the client is successfully connected to the server
    addr.sin_port = htons(c_port);

    // send a message to the server
    send_str(sock, "GET file_server", &addr);

    FILE *fp = fopen("file_client", "w");
    if (fp == NULL) {
      printf("Error opening file");
      exit(1);
    }

    unsigned int window_index = 0;
    struct segment seg;
    while (1) {
      recv_bytes(sock, (char *)&seg, sizeof(struct segment), &addr);

      if (strncmp((char *)&seg, FIN, sizeof(FIN)) == 0) {
        printf("Received FIN\n");
        break;
      }

      fwrite(seg.data, 1, seg.size, fp);

      // sleep(1);

      printf("window_index: %d\n", window_index);

      if (window_index == seg.window_size) {
        window_index = 0;

        // send ACK
        char ack_with_no[sizeof(ACK_NO) + ACK_NO_LENGTH + 1];
        sprintf(ack_with_no, "%s%d", ACK_NO, seg.no);
        send_str(sock, ack_with_no, &addr);
      }

      window_index++;
    }

    fclose(fp);

  } else {
    printf("Received unexpected message, closing connection\n");
    exit(1);
  }

  close(sock);

  return 0;
}
