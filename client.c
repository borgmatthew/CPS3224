#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "connection_common.h"

int main(int argc, char * argv[]){

	if(argc != 2){
		printf("Invalid params");
	}

	int port = atoi(argv[1]);
	int client_proxy_fd = tcp_connect("127.0.0.1", port);
	char message[20];
	
	for(;;){
		printf("Enter request: ");
		
		if(fgets(message, 20, stdin) == NULL || strcmp(message, "\n") == 0){
			printf("bye\n");
			break;
		}else{
			send(client_proxy_fd, message, strlen(message), 0);
		}
	}

	tcp_close(client_proxy_fd);
}
