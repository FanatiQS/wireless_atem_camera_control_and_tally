// Include guard
#ifndef HTTP_SOCK_H
#define HTTP_SOCK_H

int http_socket_create(char* addr);
void http_socket_await_close(int sock);
void http_socket_close(int sock);

#endif // HTTP_SOCK_H
