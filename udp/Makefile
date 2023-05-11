all: client server original_client original_server

client: client.c
		gcc -o client client.c

server: server.c
		gcc -o server server.c


original_client: original_client.c
		gcc -o original_client original_client.c

original_server: original_server.c
		gcc -o original_server original_server.c

clean:
		rm -f client server original_client original_server
