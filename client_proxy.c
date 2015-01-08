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
#include <gnutls/x509.h>
#include <strings.h>
#include <sys/epoll.h>
#include <netdb.h>

#include "proxy_common.h"
#include "connection_common.h"

static const int MAX_EVENTS = 10;
static const int MAX_BUFFER = 2 * 1024;

static int _verify_certificate_callback(gnutls_session_t session)
{
        unsigned int status;
        int ret, type;
        char * hostname;
	void *purpose = &GNUTLS_KP_TLS_WWW_SERVER;
        gnutls_datum_t out;

        /* read hostname */
        hostname = gnutls_session_get_ptr(session);

        /* This verification function uses the trusted CAs in the credentials
         * structure. So you must have installed one or more CA certificates.
         */

        gnutls_typed_vdata_st data[2];

        memset(data, 0, sizeof(data));

        data[0].type = GNUTLS_DT_DNS_HOSTNAME;
        data[0].data = (void*)hostname;

        data[1].type = GNUTLS_DT_KEY_PURPOSE_OID;
        data[1].data = purpose;

        ret = gnutls_certificate_verify_peers(session, data, 2, &status);

        if (ret < 0) {
                printf("Error\n");
                return GNUTLS_E_CERTIFICATE_ERROR;
        }

        type = gnutls_certificate_type_get(session);

        ret = gnutls_certificate_verification_status_print(status, type, &out, 0);

        if (ret < 0) {
                printf("Error\n");
                return GNUTLS_E_CERTIFICATE_ERROR;
        }

        printf("%s", out.data);

        gnutls_free(out.data);

        if (status != 0)        /* Certificate is not trusted */
                return GNUTLS_E_CERTIFICATE_ERROR;

        /* notify gnutls to continue handshake normally */
        return 0;
}

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

	if(pp.tls_enabled){
		/* for backwards compatibility with gnutls < 3.3.0 */
		gnutls_global_init();
		/* X509 stuff */
        	gnutls_certificate_allocate_credentials(&xcred);
	
        	/* Use the OS trusted ca's file */
		gnutls_certificate_set_x509_trust_file (xcred, "ca_cert.cert", GNUTLS_X509_FMT_PEM);       
        	gnutls_certificate_set_verify_function(xcred, _verify_certificate_callback);

		/* add certificate key pair */
		int cert_pair = gnutls_certificate_set_x509_key_file (xcred, "client_cert.cert", "client_key.key", GNUTLS_X509_FMT_PEM);

		if (cert_pair < 0) {
         	       printf("No certificate or key were found\n");
         	       exit(1);
        	}

	}

	/* get host details */
	struct hostent * host;
	host = gethostbyname(pp.connect_host);
	struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
	char * dest_ip = inet_ntoa(*addr_list[0]);

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
			if (ev_data->src_fd == listen_fd) {
				memset(&ev, '\0', sizeof(ep_data));

				client_addr_len = sizeof(client_addr);
				memset(&client_addr, '\0', client_addr_len );
				int conn_sock = accept(listen_fd, (struct sockaddr *) &client_addr, &client_addr_len);
				int x=fcntl(conn_sock,F_GETFL,0);
				/* set non blocking */
				fcntl(conn_sock,F_SETFL,x | O_NONBLOCK); 

				int dstfd = tcp_connect(dest_ip, pp.connect_port);
				int ret = 0;
				if(dstfd > 0) {
					int successfull_handshake = 1;
					if(pp.tls_enabled){					
						ev.session = malloc(sizeof(gnutls_session_t));

						/* Initialize session */						
						gnutls_init(ev.session, GNUTLS_CLIENT);
					
						/* set name of server */
						gnutls_session_set_ptr(* ev.session, (void *) host->h_name);

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
							if(ev_data->session != NULL) gnutls_deinit(* ev_data->session);
							close(ev_data->src_fd);
							close(ev_data->dst_fd);
							free(ev_data);
							successfull_handshake = 0;
						} else {
							char *desc;
							desc = gnutls_session_get_desc(* ev.session);
							printf("- Session info: %s\n", desc);
							gnutls_free(desc);
						}
					}
					
					if(successfull_handshake){
						fcntl(dstfd,F_SETFL,x | O_NONBLOCK);
						ev.src_fd = conn_sock;
						ev.src_enc = 0;
						ev.dst_fd = dstfd;
						if(pp.tls_enabled){ ev.dst_enc = 1; } else { ev.dst_enc = 0; }
						epoll_add(epollfd, &ev);
	
						ev.src_fd = dstfd;
						if(pp.tls_enabled){ ev.src_enc = 1; } else{ ev.src_enc = 0;}
						ev.dst_fd = conn_sock;
						ev.dst_enc = 0;
						epoll_add(epollfd, &ev);
					}
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
					if(ev_data->session != NULL) gnutls_deinit(* ev_data->session);
					close(ev_data->src_fd);
					close(ev_data->dst_fd);
					free(ev_data);
				} else {
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
