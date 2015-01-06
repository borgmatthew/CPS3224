#ifndef _PROXY_COMMON_H
#define _PROXY_COMMON_H

/* Struct to contain the typed options */
typedef struct proxy_params {
	uint16_t listen_port;
	char * connect_host;
	uint16_t connect_port;
	uint8_t tls_enabled;
} proxy_params;

/* 
 * Struct to store the source and destination file descriptors for 
 * proxied connections.
 */
typedef struct ep_data {
	int src_fd;
	uint8_t src_enc;
	int dst_fd;
	uint8_t dst_enc;
	gnutls_session_t * session;
} ep_data;

/*****************************************************************************/
/*                        Common function definitions                        */
/*****************************************************************************/

void print_help(char const * prog_name);

/*
 * Parses command line options from argc and argv into proxy_params struct pp.
 * Returns NULL on success or pointer to error message on error.
 */
const char * parse_cmd_options(int argc, char * argv[], proxy_params * pp);

/*
 * Listens on the specified port on all local addresses.
 */
int tcp_listen(int port);

int epoll_add(int epollfd, ep_data * evp);

#endif /* proxy_common.h */
