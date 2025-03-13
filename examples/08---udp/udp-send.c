/**
 * UDP client
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define BUFFER_SIZE 1400

/* returns 0 on success, -1 on timeout or error */
static int
wait_for_udp(int fd)
{
	fd_set rfds;
	struct timeval tv;
	int retval;

	/* Watch `fd` to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	/* Wait up to one seconds. */
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	retval = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (retval > 0)
		return 0;

	if (retval == -1)
		perror("select()");
	return -1; /* error or timeout */
}

int
main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "%s <server-ipv4> <port> <message>\n", argv[0]);
		return EXIT_FAILURE;
	}
	const char *srv = argv[1];
	const char *port = argv[2];
	const char *msg = argv[3];

	int ret = EXIT_FAILURE;
	int s,n = -1;
	struct addrinfo *result, *p, hints;
	char buffer[BUFFER_SIZE];
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if ((s = getaddrinfo(srv,port, &hints, &result)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}
	for(p = result; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if( s == -1 ) continue;
		n = sendto(s, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen);
		if( n != -1 ) break; // success
		close(s);
	}
	if( n == -1 ) {
		perror( "sendto error" );
		freeaddrinfo(result);
		exit( EXIT_FAILURE );
	}
	
	if (!wait_for_udp(s) && 
	    (n = recvfrom(s, buffer, BUFFER_SIZE, 0, NULL, NULL ))) {
		buffer[n] = '\0'; // Null-terminate the received string
		puts( buffer );
		ret = EXIT_SUCCESS;
	}
	close(s); freeaddrinfo(result); return ret; 
}
