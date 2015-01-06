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

	gnutls_certificate_credentials_t xcred;

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
	/* X509 stuff */
        gnutls_certificate_allocate_credentials(&xcred);

        /* Use the OS trusted ca's file */
        gnutls_certificate_set_x509_system_trust(xcred);
        /*gnutls_certificate_set_verify_function(xcred,
                                              _verify_certificate_callback);*/


	ep_data ev;
	epollfd = epoll_create(MAX_EVENTS);
	listen_fd = tcp_listen(pp.listen_port);
	ev.src_fd = listen_fd;
	ev.src_enc = 0;
	ev.dst_fd = 0;
	ev.dst_enc = 0;
	ev.session = NULL;
	epoll_add(epollfd, &ev);


	/*************************** Event loop ******************************/

	for (;;) {
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (int n = 0; n < nfds; ++n) {
			ep_data * ev_data = events[n].data.ptr;
			printf("Epoll[%d] triggering fd %d\n", n, ev_data->src_fd);
			if (ev_data->src_fd == listen_fd) {
				memset(&ev, '\0', sizeof(ep_data));

				printf("Got new connection\n");
				client_addr_len = sizeof(client_addr);
				memset(&client_addr, '\0', client_addr_len );
				int conn_sock = accept(listen_fd, (struct sockaddr *) &client_addr, &client_addr_len);
				int x=fcntl(conn_sock,F_GETFL,0);
				/* set non blocking */
				fcntl(conn_sock,F_SETFL,x | O_NONBLOCK); 

				int dstfd = tcp_connect(pp.connect_host, pp.connect_port);
				int ret = 0;
				if(dstfd > 0) {
					ev.session = malloc(sizeof(gnutls_session_t));
					gnutls_init(ev.session, GNUTLS_CLIENT);

				        /* use default priorities */
				        gnutls_set_default_priority(* ev.session);

					/* put the x509 credentials to the current session */
				        gnutls_credentials_set(* ev.session, GNUTLS_CRD_CERTIFICATE, xcred);

					gnutls_transport_set_int(* ev.session, dstfd);
					gnutls_handshake_set_timeout(* ev.session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
					/* Perform the TLS handshake */
					do {
						ret = gnutls_handshake(* ev.session);
					}
					while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
					if (ret < 0) {
						fprintf(stderr, "*** Handshake failed\n");
						gnutls_perror(ret);
					} else {
						char *desc;
						desc = gnutls_session_get_desc(* ev.session);
						printf("- Session info: %s\n", desc);
						gnutls_free(desc);
					}

					fcntl(dstfd,F_SETFL,x | O_NONBLOCK);
					ev.src_fd = conn_sock;
					ev.src_enc = 0;
					ev.dst_fd = dstfd;
					ev.dst_enc = 1;
					epoll_add(epollfd, &ev);

					ev.src_fd = dstfd;
					ev.src_enc = 1;
					ev.dst_fd = conn_sock;
					ev.dst_enc = 0;
					epoll_add(epollfd, &ev);
					printf("src connection fd: %d, dest connection fd: %d\n", conn_sock, dstfd);
				} else {
					close(conn_sock);
				}
			} else {
				char message[MAX_BUFFER + 1];
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
					|| (ev_data->src_enc && (rec_length = gnutls_record_recv(* ev_data->session, message, MAX_BUFFER)) == 0)
					|| (!(ev_data->src_enc) && (rec_length = recv(ev_data->src_fd, message, MAX_BUFFER, 0)) == 0)
				)
				{
					printf("Closing fds src[%d], dst[%d]\n", ev_data->src_fd,ev_data->dst_fd);
					if(ev_data->session != NULL) gnutls_deinit(* ev_data->session);
					close(ev_data->src_fd);
					close(ev_data->dst_fd);
					free(ev_data);
				} else {
					printf("Got [%d] bytes from %d [%d] to %d [%d]\n[%s]\n",rec_length,ev_data->src_fd, ev_data->src_enc, ev_data->dst_fd, ev_data->dst_enc,message);
					/** encrypt and send **/
					if(ev_data->dst_enc) {
						gnutls_record_send(* ev_data->session, message, rec_length);
					} else {
						send(ev_data->dst_fd, message, rec_length, 0);
					}
				}
			}
		}
	}
}
