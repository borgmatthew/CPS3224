#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <gnutls/gnutls.h>
#include <strings.h>
#include <sys/epoll.h>

#include "proxy_common.h"
#include "connection_common.h"

static const int MAX_EVENTS = 10;
static const int MAX_BUFFER = 2 * 1024;

int main(int argc, char * argv[]) {
	int listen_fd = -1;
	proxy_params pp;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	struct epoll_event events[MAX_EVENTS];
	int epollfd = -1;


	/************************** Get Params *******************************/

	const char * parse_error = parse_cmd_options(argc, argv, &pp);
	if(parse_error != NULL) {
		fprintf(stderr, "%s\r\n", parse_error);
		print_help(argv[0]);
		exit(-1);
	}

	printf("listen_port: [%d], connect_host: [%s], connect_port: [%d], tls_enabled: [%d]\n",
		pp.listen_port, pp.connect_host, pp.connect_port, pp.tls_enabled);

	/******************************* Init ********************************/

	/* for backwards compatibility with gnutls < 3.3.0 */
	gnutls_global_init();

	epollfd = epoll_create(MAX_EVENTS);
	listen_fd = tcp_listen(pp.listen_port);
	epoll_add(epollfd, listen_fd, 0);

	for (;;) {
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (int n = 0; n < nfds; ++n) {
			ep_data * ev_data = events[n].data.ptr;
			printf("Epoll[%d] triggering fd %d\n", n, ev_data->src_fd);
			if (ev_data->src_fd == listen_fd) {
				printf("Got new connection\n");
				client_addr_len = sizeof(client_addr);
				memset(&client_addr, '\0', client_addr_len );
				int conn_sock = accept(listen_fd, (struct sockaddr *) &client_addr, &client_addr_len);
				int x=fcntl(conn_sock,F_GETFL,0);
				/* set non blocking */
				fcntl(conn_sock,F_SETFL,x | O_NONBLOCK); 

				int dstfd = tcp_connect(pp.connect_host, pp.connect_port);
				if(dstfd > 0) {
					fcntl(dstfd,F_SETFL,x | O_NONBLOCK);
					epoll_add(epollfd, conn_sock, dstfd);
					epoll_add(epollfd, dstfd, conn_sock);
					printf("src connection fd: %d, dest connection fd: %d\n", conn_sock, dstfd);
				} else {
					close(conn_sock);
				}
			} else {
				char message[MAX_BUFFER];
				bzero(message, MAX_BUFFER);
				int rec_length = 0;
				/*
 				 * Close source and dest if there is an error
 				 * or a 0 length read.
 				 */
				if ((events[n].events & EPOLLERR) 
					|| (events[n].events & EPOLLHUP) 
					|| (events[n].events & EPOLLRDHUP) 
					|| (!(events[n].events & EPOLLIN))
					|| ((rec_length = recv(ev_data->src_fd, message, MAX_BUFFER, 0)) == 0))
				{
					printf("Closing fds src[%d], dst[%d]\n", ev_data->src_fd,ev_data->dst_fd);
					close(ev_data->src_fd);
					close(ev_data->dst_fd);
					free(ev_data);
				} else {
					printf("Got [%d] bytes from %d to %d\n",rec_length,ev_data->src_fd, ev_data->dst_fd);
					/** encrypt and send **/
					send(ev_data->dst_fd, message, rec_length, 0);
				}
			}
		}
	}
}
