#ifndef _CONNECTION_COMMON_H
#define _CONNECTION_COMMON_H

#include <arpa/inet.h>

/*
Connects to a host over the specified port
*/
int tcp_connect(const char *connect_host, uint16_t connect_port);

/** close connection **/
void tcp_close(int sd);

#endif
