/* 
 Initial version: written by Ken Harris 
*/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <arpa/inet.h>  
#include <netinet/in.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>

#define DEFAULT_PORT 28015

inline int min(int a, int b) { return a < b ? a : b; }

char rbuff[0x400000];
char sbuff[0x1000];

int main(int argc, char * argv[])
{
	int sock, listen_sock;
	int verbose = 0;
	char sport[16];
	struct addrinfo *result = NULL;
	struct addrinfo hints;
	int ret;
	int done_and_exit = 0;

	if (argc > 1 ) {
		verbose = 1;
	}
	
	memset(sbuff, 'Z', sizeof(sbuff));
	memset(rbuff, 'Z', sizeof(rbuff));

AGAIN: 
	sock = -1;
	listen_sock = -1;
	result = NULL;
	printf("LISTEN...\n");
	memset(&hints, 0x00, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	sprintf(sport, "%d", DEFAULT_PORT);
	ret = getaddrinfo(NULL, sport, &hints, &result);
	if ( ret != 0 ) {
		printf("ERROR: getaddrinfo returned %d\n", ret); 
		exit(1);
	}
	listen_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_sock == -1) {
		perror("socket");
		done_and_exit = 1;
		goto DONE;
	}	
	ret = bind(listen_sock, result->ai_addr, (int)result->ai_addrlen);
	if (ret == -1) {
		perror("bind");
		done_and_exit = 1;
		goto DONE;
	}	
	freeaddrinfo(result);
	result = NULL;
	ret = listen(listen_sock, 1);
	if (ret == -1) {
		perror("listen");
		done_and_exit = 1;
		goto DONE;
	}
	sock = accept(listen_sock, NULL, NULL);						
	if (sock == -1) {
		perror("accept");
		done_and_exit = 1;
		goto DONE;
	}
	shutdown(listen_sock, SHUT_RDWR);
	close(listen_sock);
	listen_sock = -1;
	while (1) {
		int got;
		int r;
		/* first 4 bytes is the length */
		int rx = 4;
		while (rx) {
			r = recv(sock, &rbuff[4-rx], rx, 0);
			if (r < 0) {
				perror("recv");
				goto DONE;
			} else if (r > 0) {
				rx -= r;
			} else {
				printf("recv-1: Nothing received.\n");
				goto DONE;
			}
		}
		rx = *(unsigned int *) &rbuff[0];
		rx -= 4;
		if (verbose) printf("EXPECT: %d bytes\n", rx);
		if (rx == 0) {
			printf("Size zero, quitting ...\n");
			done_and_exit = 1;
			goto DONE;
		}
		/* receive data */
		got = 4;
		while (rx) {
			r = recv(sock, &rbuff[got], min(rx, 2 * 1024 * 1024), 0);
			if (r < 0) {
				perror("recv");
				goto DONE;
			} else if (r > 0) {
				rx -= r;
				got += r;
				if (verbose) printf("Received %d, %d done, %d to go\n", r, got, rx);
			} else {
				printf("recv-2: Nothing received.\n");
				goto DONE;
  			}
		}
		/* send response back to client */
		int sz = 80;
		int tx = 0;
		*(int *) &sbuff[0] = sz + sizeof(int);
		tx = sz + sizeof(int);
		if (verbose) printf("ACK: %d bytes\n", tx);
		while(tx) {
			r = send(sock, &sbuff[sz + sizeof(int) - tx], tx, 0);
			if (r < 0) {
				perror("send");
				goto DONE;
			} else if (r > 0) {
				tx -= r;
			} else {
				printf("send: sent none\n");
				goto DONE;
			}
		}
	}

DONE:
	if(result != NULL) freeaddrinfo(result);
	if(listen_sock != -1) {
		shutdown(listen_sock, SHUT_RDWR);
		close(listen_sock);
	}
	if(sock != -1) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	if(done_and_exit) exit(2);
	sleep(1);
	goto AGAIN;
}

/*
On illumos: gcc -lnsl -lsocket -lstdc++ -o tcp-server tcp-server.cpp
On Linux: gcc -o tcp-server tcp-server.cpp
*/
