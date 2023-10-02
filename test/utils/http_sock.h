// Include guard
#ifndef HTTP_SOCK_H
#define HTTP_SOCK_H

#include <stddef.h> // size_t

int http_socket_create(void);
void http_socket_close(int sock);

void http_socket_write(int sock, const char* buf, size_t len);
void http_socket_send(int sock, const char* str);

size_t http_socket_recv(int sock, char* buf, size_t size);
size_t http_socket_recv_len(int sock);

void http_socket_recv_cmp(int sock, const char* cmpBuf);
char* http_status(int code);
void http_socket_recv_cmp_status_line(int sock, int code);

void http_print(char* str);
void http_socket_recvprint(int sock);

void http_socket_recv_close(int sock);
void http_socket_recv_flush(int sock);

#endif // HTTP_SOCK_H
