CC = gcc
CFLAGS = -Wall

all: udp_client udp_server

uftp_client: client/udp_client.c
	$(CC) $(CFLAGS) -o udp_client client/udp_client.c

uftp_server: server/udp_server.c
	$(CC) $(CFLAGS) -o udp_server server/udp_server.c

clean:
	rm -f udp_client udp_server

