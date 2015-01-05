#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "connection_common.h"

int tcp_connect(const char *connect_host, uint16_t connect_port)
{
        int err, sd;
        struct sockaddr_in sa;

        /* connects to server
         */
        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&sa, '\0', sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(connect_port);
        inet_pton(AF_INET, connect_host, &sa.sin_addr);

        err = connect(sd, (struct sockaddr *) &sa, sizeof(sa));
        if (err < 0) {
                fprintf(stderr, "Connect error\n");
		sd = -1;
        }

        return sd;
}

void tcp_close(int sd)
{
        shutdown(sd, SHUT_RDWR);        /* no more receptions */
        close(sd);
}
