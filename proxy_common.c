#ifndef _PROXY_COMMON_C
#define _PROXY_COMMON_C

#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <gnutls/gnutls.h>
#include "proxy_common.h"

#define CONNECTION_BACKLOG 1000

static const char * const MISSING_REQUIRED_ARGUMENTS = "Missing required arguments!";
static const char * const INVALID_LISTEN_PORT = "Invalid listening port!";
static const char * const INVALID_CONNECT_PORT = "Invalid connect port!";
static const char * const INVALID_CONNECT_HOST = "Invalid connect host!";
static const char * const INVALID_ARGUMENT = "Invalid argument passed!";

/* Common proxy command line options */
static struct option proxy_options[] = {
	{"listen_port",  required_argument, NULL,  0 },
	{"connect_host", required_argument, NULL,  1 },
	{"connect_port", required_argument, NULL,  2 },
	{"tls",          required_argument, NULL,  3 },
	{0,              0,                 0,  0 }
};


void print_help(char const * prog_name)
{
	printf("\n");
	printf("%s --listen_port port --connect_host hostname --connect_port port [--tls {on,off}]\n", prog_name);
}


const char * parse_cmd_options(int argc, char * argv[], proxy_params * pp)
{
	const char * error = NULL;
	int required_args_cnt = 0;

	int c, tmp_port;
	size_t hostname_len;

	memset(pp, '\0', sizeof(proxy_params));

	/* set default value */
	pp->tls_enabled = 1;


	while((c = getopt_long(argc, argv, "", proxy_options, NULL)) != -1 && error == NULL) {
		switch(c) {
			case 0 :
				tmp_port = strtol(optarg, NULL, 10);
				if(tmp_port < 1 || tmp_port > 65536) {
					error = INVALID_LISTEN_PORT;
				} else {
					pp->listen_port = (uint16_t) tmp_port;
					required_args_cnt++;
				}
				break;
			case 1 :
				hostname_len = strlen(optarg);
				/* allocate space for null byte */
				pp->connect_host = malloc(hostname_len+1);
				strncpy(pp->connect_host, optarg, hostname_len);
				pp->connect_host[hostname_len] = 0;
				required_args_cnt++;
				break;
			case 2 :
				tmp_port = strtol(optarg, NULL, 10);
				if(tmp_port < 1 || tmp_port > 65536) {
					error = INVALID_CONNECT_PORT;
				} else {
					pp->connect_port = (uint16_t) tmp_port;
					required_args_cnt++;
				}
				break;
			case 3 :
				if(strncmp("off",optarg,3) == 0) {
					pp->tls_enabled = 0;
				} else {
					pp->tls_enabled = 1;
				}
				break;
			default : 
				error = INVALID_ARGUMENT;
				break;
		}
	}

	if(required_args_cnt < 3 && error == NULL) {
		error = MISSING_REQUIRED_ARGUMENTS;
	}

	return error;
}

int tcp_listen(int port)
{
	struct sockaddr_in listen_addr;

	int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&listen_addr, '\0', sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(port);
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr));
	listen(listen_fd, CONNECTION_BACKLOG);
	return listen_fd;
}

int epoll_add(int epollfd, ep_data * evp)
{
	struct epoll_event ev;

	memset(&ev, '\0', sizeof(ev));
	ev.events = EPOLLIN | EPOLLRDHUP;
	ev.data.ptr = malloc(sizeof(ep_data));
	((ep_data *) ev.data.ptr)->src_fd = evp->src_fd;
	((ep_data *) ev.data.ptr)->src_enc = evp->src_enc;
	((ep_data *) ev.data.ptr)->dst_fd = evp->dst_fd;
	((ep_data *) ev.data.ptr)->dst_enc = evp->dst_enc;
	((ep_data *) ev.data.ptr)->session = evp->session;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, evp->src_fd, &ev);
	return 0;
}

#endif /* proxy_common.c */
