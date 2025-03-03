/**
 * UDP server
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include "mls.h"


/*
1) Store values in group Hostname:
   recv:  hostname {key=value }*
          value= 'eqstring' | "eqqstring" | esimplestring
	  esimplestring = [^'" ]*  with escaped chars
	  eqstring  =  [^']* with escaped chars
	  eqqstring =  [^"]* with escaped chars
   max: 1470 BYTES / Packet
   
2) retrive values
   recv:   hostname *
   send:   {key='escaped-string' SPACE}* NEWLINE <terminates session>
   max: 1470 BYTES / Packet


   parser:

   w1 = word until not "="

   w2 = c = next-char
   case ':  scan-with-esc-until-next-quote
   case ":  scan-with-esc-until-next-doublequote
   default: scan-with-esc-until-next-whitespace
   store(hostname, w1, w2 )
   until no more characters  
*/

/*
  db: host - keystore
  keystore ( key,value )
*/




typedef unsigned long uint64;
typedef unsigned char uint8;

#define BUF_SIZE 1000

#define ALARM_RESET 20
#define INTV_AVG 7
#define INTV_TIME 5
#define INTV_LEVEL 3
int intv_secs[INTV_LEVEL]   = { 6, 8, 11 };

uint64 diff_timespec(const struct timespec *time1,
    const struct timespec *time0) {

  struct timespec diff = {.tv_sec = time1->tv_sec - time0->tv_sec, //
      .tv_nsec = time1->tv_nsec - time0->tv_nsec};
  if (diff.tv_nsec < 0) {
    diff.tv_nsec += 1000000000; // nsec/sec
    diff.tv_sec--;
  }

  uint64 ret = diff.tv_sec * 1000 + diff.tv_nsec / 1e6;
  return ret;
}

struct client_tm {
  uint8 cnt, avg_init;
  uint64 delta[INTV_AVG];
  struct timespec last;
  int alarm;
  int alarm_timer;
  uint64 jitter_sum, avg;

} CL_TEST = { 0 };





void msg_client( char *host, char *buf )
{
  int i;
  uint64 diff;
  struct timespec cur;
  struct client_tm *cl = & CL_TEST;
  clock_gettime( CLOCK_MONOTONIC_RAW, &cur);
  if( ! host ) { return; }
  printf( "*%s:%f\n", host, cur.tv_sec * 1000 + cur.tv_nsec / 1e6 );
  return;

  diff = diff_timespec( &cur, & cl->last );
  printf("ping %ld\n", diff );

  /* noch keine startzeit gesetzt */
  if( cl->last.tv_sec + cl->last.tv_nsec == 0 ) {
    cl->last = cur;
    return;
  }

  /* wennn keine Nachricht kommt wird ein alarm ausgeloest
     Der alarm wird eskaliert, wenn  laenger keine Nachricht kommt
  */
  if( ! buf ) {
    /* keine nachricht, aber noch zu frueh zum meckern */
    if( diff < intv_secs[0] * 1000 ) return;

    /* nachricht ueberfaellig, muss mal etwas laut werden */
    cl->cnt=0; cl->avg_init=0; /* jitter loeschen */
    cl->alarm_timer = ALARM_RESET;
    for( i = 1; i <= INTV_LEVEL; i++ ) {
      if( diff > intv_secs[ INTV_LEVEL - i] * 1000 ) {
        printf("ALARM: %d %ld\n", i, diff );
        cl->alarm = i;
        break;
      }
    }
    return;
  }


  puts("msg");
  cl->last = cur;

  /* wurde ein alarm ausgeloest wird dieser bei der naechsten nachricht eine stufe runtergestuft */
  if( cl->alarm ) cl->alarm--;

  /* wenn eine Nachricht ankommt kann der jitter berechnet werden
     Die genauigkeit betraegt 10 milliseconds.
     Voraussetzung: Es sind INTV_AVG Nachrichten vorhanden
     Formel: floating_avg( summme der quadrierte abweichungen ) / n
  */

  /* quadrat der abweichung in ms */
  uint64 tm = diff / 10;
  tm -= INTV_TIME * 100;
  tm *= tm;

  /* floating average */
  cl->jitter_sum += tm ;
  cl->jitter_sum -= cl->delta[cl->cnt];
  cl->delta[cl->cnt] = tm;

  /* Die Auswertung kann erfolgen sobald mindestens INT_AVG points gesammelt wurden */
  printf("cnt=%d\n", cl->cnt );
  cl->cnt++; if( cl->cnt >= INTV_AVG ) {
    cl->cnt=0;
    cl->avg_init=1;
  }
  if( ! cl->avg_init ) return;

  uint64 avg = cl->jitter_sum / INTV_AVG;
  printf("jitter %ld\n", avg  );
  cl->avg = avg;
}


int wait_for_udp(int fd)
{
  fd_set rfds;
  struct timeval tv;
  int retval;

  /* Watch stdin (fd 0) to see when it has input. */

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /* Wait up to one seconds. */
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  retval = select(fd+1, &rfds, NULL, NULL, &tv);
  if (retval == -1)
    perror("select()");
  else if (retval)
    return 0;
  return 1;
}


int main(int argc, char *argv[])
{
	m_init();
	trace_level=1;

	struct addrinfo hints;
           struct addrinfo *result, *rp;
           int sfd, s;
           struct sockaddr_storage peer_addr;
           socklen_t peer_addr_len;
           ssize_t nread;
           char buf[BUF_SIZE];

           if (argc != 2) {
               fprintf(stderr, "Usage: %s port\n", argv[0]);
               exit(EXIT_FAILURE);
           }

           memset(&hints, 0, sizeof(struct addrinfo));
           hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
           hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
           hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
           hints.ai_protocol = 0;          /* Any protocol */
           hints.ai_canonname = NULL;
           hints.ai_addr = NULL;
           hints.ai_next = NULL;

           s = getaddrinfo(NULL, argv[1], &hints, &result);
           if (s != 0) {
               fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
               exit(EXIT_FAILURE);
           }

           /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully bind(2).
              If socket(2) (or bind(2)) fails, we (close the socket
              and) try the next address. */

           for (rp = result; rp != NULL; rp = rp->ai_next) {
               sfd = socket(rp->ai_family, rp->ai_socktype,
                       rp->ai_protocol);
               if (sfd == -1)
                   continue;

               if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
                   break;                  /* Success */

               close(sfd);
           }

           if (rp == NULL) {               /* No address succeeded */
               fprintf(stderr, "Could not bind\n");
               exit(EXIT_FAILURE);
           }

           freeaddrinfo(result);           /* No longer needed */

           /* Read datagrams and echo them back to sender */


           for (;;) {
               peer_addr_len = sizeof(struct sockaddr_storage);
               if( wait_for_udp( sfd ) != 0 ||
                   (nread=recvfrom(sfd, buf, BUF_SIZE, 0,
                                    (struct sockaddr *) &peer_addr, &peer_addr_len)) <= 0 )
                 {
                   msg_client(0,0);
                   continue;
                 }

               char host[NI_MAXHOST], service[NI_MAXSERV];

               s = getnameinfo((struct sockaddr *) &peer_addr,
                               peer_addr_len, host, NI_MAXHOST,
                               service, NI_MAXSERV, NI_NUMERICSERV);
               if (s == 0) {

                 msg_client(host,buf);
                 printf("Received %zd bytes from %s:%s\n",
                        nread, host, service);
               }
               else
                   fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
           }

	   m_destruct();
}
