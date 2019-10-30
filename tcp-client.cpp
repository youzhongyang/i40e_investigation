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
#define INVALID_SOCKET -1
#define SOCKET int
#define uint64_t unsigned long
#define boolean_t unsigned short
#define TRUE ((boolean_t)1)
#define FALSE ((boolean_t)0)

int cnt_fast_transfers = 0;
int cnt_retries = 0;
uint64_t total_bytes_sent = 0;

/*
with 6 * 65652 bytes write requests, and then a 131188 bytes write request reproduces the delayed ~310ms rx packet on the server
*/

char sbuff[0x300000];
char rbuff[0x1000];

uint64_t gettime_ms() {
	struct timespec ts;
	uint64_t ms = 0;
	clock_gettime( CLOCK_REALTIME, &ts );
	ms = ts.tv_nsec / 1000000;
	ms += ts.tv_sec * 1000;
	return ms;
}

boolean_t send_bytes(SOCKET s, int size)
{
	int tx = 0;
	int r = 0;
	
	*(int *) &sbuff[0] = size;
	tx = size;
	while(tx) {
		r = send(s, &sbuff[size - tx], tx, 0);
		if (r < 0) {
			perror("send");
			return (FALSE);
		} else if (r > 0) {
			total_bytes_sent += r;
			tx -= r;
		} else {
			printf("send: zero byte\n");
			exit(1);
		}
	}
	return (TRUE);
}

boolean_t receive_bytes(SOCKET s, int size)
{
	int rx = size;
	int r = 0;
	while (rx) {
		r = recv(s, rbuff, rx, 0);
		if (r < 0) {
			perror("recv");
			return (FALSE);
		} else if (r > 0) {
			rx -= r;
		} else {
			printf("recv: zero byte\n");
			exit(1);
		}
	}	
	return (TRUE);
}

int main(int argc, char* argv[])
{
	uint64_t last, now, diff;
	int r = 0;
	SOCKET sock;
	struct sockaddr_in addr;
	struct hostent *remoteHost;
	int initial_request_size = 65652;
	int request_size = 131188;

	if (argc < 2) {
		printf("usage: %s <host> [initial request size] [request size]\n", argv[0]);
		exit(1);
	}
	
	if(argc > 2) {
		initial_request_size = atoi(argv[2]);
		if(initial_request_size <= 0 || initial_request_size > 0x200000)
			initial_request_size = 65652;
	}
	if(argc > 3) {
		request_size = atoi(argv[3]);
		if(request_size <= 0 || request_size > 0x200000)
			request_size = 131188;
	}

	printf("initial request size = %d\n", initial_request_size);
	printf("request size = %d\n", request_size);
	
	memset(sbuff, 'Z', sizeof(sbuff));

AGAIN:
	sock = INVALID_SOCKET;

	remoteHost = gethostbyname(argv[1]);
	if (remoteHost == NULL) {
		fprintf(stderr, "ERROR: gethostbyname returned %d\n", h_errno);
		return (-1);
	} else {
		if (remoteHost->h_addrtype != AF_INET) {		
			fprintf(stderr, "ERROR: not AF_INET - %s\n", argv[1]);
			return (-1);
		}
	}	
	addr.sin_family = AF_INET;
	*(u_long *) &addr.sin_addr = *(u_long *) remoteHost->h_addr_list[0];				
	addr.sin_port = htons(DEFAULT_PORT);				

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		perror("socket");
		goto DONE;
	}
	
	r = connect(sock, (const sockaddr *) &addr, sizeof(struct sockaddr_in));
	if (r != 0) {
		perror("connect");
		goto DONE;
	}
	printf("CONNECTED\n");
	
	last = gettime_ms();
	for(int i = 0; i < 6; i++)
	{
		/* send 65536+116 bytes */
		if(!send_bytes(sock, initial_request_size)) goto DONE;
		/* receive response */
		if(!receive_bytes(sock, 84)) goto DONE;
	}
	while (1) {
		/* send 131072+116 bytes */
		if(!send_bytes(sock, request_size)) 
			goto DONE;
		/* receive response */
		if(!receive_bytes(sock, 84))
			goto DONE;
		now = gettime_ms();
		if (now > last + 1000) {
			diff = now - last;
			last = now;
			printf("%d K in %ld ms\n", total_bytes_sent/(1024), diff);
			if (total_bytes_sent > 100 * 1024 * 1024) {
				cnt_fast_transfers++;
				if (cnt_fast_transfers > 3) {
					printf("Too fast, retry: %d\n", ++cnt_retries);
					cnt_fast_transfers = 0;
					goto DONE;
				}
			} else cnt_fast_transfers = 0;
			total_bytes_sent = 0;
		}
	}

DONE:
	if(sock != INVALID_SOCKET) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	sleep(2);
	goto AGAIN;

	return 0;
}

/*
On illumos: gcc -lnsl -lsocket -lstdc++ -o tcp-client tcp-client.cpp
On Linux: gcc -o tcp-client tcp-client.cpp
*/
