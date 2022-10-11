#ifndef SHARED_H
#define SHARED_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DOMAIN AF_INET

#define MSG_LENGTH 64
#define PORT_LENGTH 5
#define ACK_NO_LENGTH 5
#define FILE_CHUNK_SIZE 1024

#define SYN "SYN"
#define SYN_ACK "SYN-ACK "
#define ACK "ACK"
#define GET "GET"
#define ACK_NO "ACK_"
#define FIN "FIN"

struct file_segment_with_no {
  unsigned short no;
  size_t size;
  char data[FILE_CHUNK_SIZE];
};

void checkerr(long err, char *msg) {
  if (err < 0) {
    printf("err = %ld | %s\n", err, msg);
    exit(1);
  }
}

long my_send_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  printf("Sending \"%s\" to %s:%d\n", msg, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, msg, strlen(msg) + 1, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "my_send_str");
  return n;
}

long my_send_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  printf("Sending %ld bytes to %s:%d\n", len, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  ssize_t n = sendto(s, buffer, len, 0, (struct sockaddr *)addr_ptr, sizeof(struct sockaddr_in));
  checkerr(n, "my_send_bytes");
  return n;
}

long my_recv_str(int s, char *msg, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, msg, MSG_LENGTH, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "my_recv_str");
  printf("Received \"%s\" from %s:%d\n", msg, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  return n;
}

long my_recv_bytes(int s, char *buffer, size_t len, struct sockaddr_in *addr_ptr) {
  socklen_t size = sizeof(struct sockaddr_in);
  ssize_t n = recvfrom(s, buffer, len, 0, (struct sockaddr *)addr_ptr, &size);
  checkerr(n, "my_recv_bytes");
  printf("Received %ld bytes from %s:%d\n", n, inet_ntoa(addr_ptr->sin_addr), ntohs(addr_ptr->sin_port));
  return n;
}

long recv_control_str(int s, char *control_str, struct sockaddr_in *addr_ptr) {
  printf("Waiting for \"%s\" on socket %d...\n", control_str, s);

  char msg[MSG_LENGTH];
  long n = my_recv_str(s, msg, addr_ptr);

  if (strncmp(msg, control_str, strlen(control_str)) != 0) {
    printf("Expected %s, got %s\n", control_str, msg);
    return 0;
  }

  return n;
}

#endif
