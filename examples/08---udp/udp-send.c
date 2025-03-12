/**
 * UDP client
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

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

int main(int argc, char **argv)
{
	if( argc < 4 ) {
		fprintf(stderr, "%s <server-ipv4> <port> <message>\n", argv[0] );
		return EXIT_FAILURE;
	}
	
	char *srv = argv[1]; int port = atoi(argv[2]); char *msg = argv[3];	
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror( "Error opening socket" );
		return EXIT_FAILURE;
	}
	struct sockaddr_in server = { .sin_family = AF_INET, .sin_addr.s_addr = inet_addr(srv), .sin_port = htons(port) };
	if (sendto(sockfd, msg, strlen(msg)+1, 0,
		   (const struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror( "Error in sendto()");
		return EXIT_FAILURE;
	}

	int n=0;
	#define BUFFER_SIZE 1400
	char buffer[BUFFER_SIZE];
	socklen_t addr_len = sizeof(server);
	
	if( wait_for_udp(sockfd) || 
	    (n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server, &addr_len )) <= 0 ) {	     
		perror("Receive failed");
		close(sockfd);
		exit(1);
	}
	
	buffer[n] = '\0'; // Null-terminate the received string
	printf("Response from server: %s\n", buffer);
	close(sockfd);

	return EXIT_SUCCESS;
}
