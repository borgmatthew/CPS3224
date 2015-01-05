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

static const int CONNECTION_BACKLOG = 1000;
static const int MAX_EVENTS = 10;
static const int MAX_BUFFER = 1024;

int main(int argc, char * argv[]) {
	int listen_fd = -1;
	proxy_params pp;
	struct sockaddr_in listen_addr, client_addr;
	socklen_t client_addr_len;


	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd = -1;

	/** Get params **/

	const char * parse_error = parse_cmd_options(argc, argv, &pp);
	if(parse_error != NULL) {
		fprintf(stderr, "%s\r\n", parse_error);
		print_help(argv[0]);
		exit(-1);
	}

	printf("%d, %s, %d, %d\n",pp.listen_port, pp.connect_host, pp.connect_port, pp.tls_enabled);

	/** Init **/

	/* for backwards compatibility with gnutls < 3.3.0 */
	gnutls_global_init();

	epollfd = epoll_create(MAX_EVENTS);


	/** Setup Ports **/
	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&listen_addr, '\0', sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(pp.listen_port);
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr));
	listen(listen_fd, CONNECTION_BACKLOG);


	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_fd, &ev);

	/** Process Connections **/


	/** Open / reuse connection **/


	for (;;) {
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (int n = 0; n < nfds; ++n) {
			if (events[n].data.fd == listen_fd) {
				client_addr_len = sizeof(client_addr);
				memset(&client_addr, '\0', client_addr_len );
				int conn_sock = accept(listen_fd, (struct sockaddr *) &client_addr, &client_addr_len);
				int x=fcntl(conn_sock,F_GETFL,0);
				fcntl(conn_sock,F_SETFL,x | O_NONBLOCK);
				//setnonblocking(conn_sock);
				ev.events = EPOLLIN;
				ev.data.fd = conn_sock;
				epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,&ev);

				// Open destination connection
				printf("opening connection\n");
				events[n].data.u32 = tcp_connect(pp.connect_host, pp.connect_port);
			} else {
				printf("Receiving\n");
				//do_use_fd(events[n].data.fd);
				char message[MAX_BUFFER];
				bzero(message, MAX_BUFFER);
				int rec_length = 0;
				while((rec_length = recv(events[n].data.fd, message, MAX_BUFFER, 0)) > 0){
					printf("%s, %i\n nfds:%i\ntest:%i", message, rec_length, nfds, events[n].data.u32);
					fflush(stdout);
				}
				if(events[n].events == EPOLLRDHUP){
					close(events[n].data.fd);
					close(events[n].data.u32);
				}
			}
		}
	}

	/** encrypt and send **/
}
