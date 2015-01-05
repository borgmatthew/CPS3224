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
	int dst_fd;
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

#endif /* proxy_common.h */
