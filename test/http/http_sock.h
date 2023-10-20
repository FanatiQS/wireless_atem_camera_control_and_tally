// Include guard
#ifndef HTTP_SOCK_H
#define HTTP_SOCK_H

#include <stddef.h> // size_t
#include <stdio.h> // FILE

int http_socket_create(void);

void http_socket_write(int sock, const char* buf, size_t len);
void http_socket_send(int sock, const char* str);

size_t http_socket_recv(int sock, char* buf, size_t size);
size_t http_socket_recv_len(int sock);

void http_socket_recv_cmp(int sock, const char* cmpBuf);
char* http_status(int code);
void http_socket_recv_cmp_status(int sock, int code);

void http_print(char* str, FILE* pipe);
void http_socket_recvprint(int sock);

void http_socket_recv_close(int sock);
void http_socket_recv_flush(int sock);
void http_socket_recv_error(int sock, int err);

void http_socket_close(int sock);

void http_socket_body_write(int sock, const char* body, size_t bodyLen);
void http_socket_body_send(int sock, const char* body);

#endif // HTTP_SOCK_H
