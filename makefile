CLIENT_PROXY_OBJS := proxy_common.o connection_common.o
SERVER_PROXY_OBJS := proxy_common.o connection_common.o
CLIENT_OBJS := connection_common.o
CFLAGS = -W -Wall -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wswitch-enum -fno-common -Wnested-externs -O2 -pedantic -std=c99
LINK_FLAGS = -lgnutls
DEBUG_FLAGS = -g
CC = gcc

%.o: %.c %.h
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(LINK_FLAGS) -o $@ -c $<

all: client_proxy server_proxy client

server_proxy: server_proxy.c $(SERVER_PROXY_OBJS) 
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(LINK_FLAGS) -o $@ server_proxy.c $(SERVER_PROXY_OBJS) `pkg-config gnutls --cflags --libs` 

client_proxy: client_proxy.c $(CLIENT_PROXY_OBJS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(LINK_FLAGS) -o $@ client_proxy.c $(CLIENT_PROXY_OBJS) `pkg-config gnutls --cflags --libs` 

client: client.c $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(LINK_FLAGS) -o $@ client.c $(CLIENT_OBJS)

clean:
	rm -f client_proxy server_proxy client $(SERVER_PROXY_OBJS) $(CLIENT_PROXY_OBJS) $(CLIENT_OBJS)
