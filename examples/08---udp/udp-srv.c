/**
 * UDP server
 */
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"

/* lookup_obj = create(v,ctx) */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include "mls.h"
#include "m_tool.h"


/*
1) Store values in group Hostname:
   recv:  hostname {key=value }*
          value= 'eqstring' | "eqqstring" | esimplestring
	  esimplestring = [^'" ]*  with escaped chars
	  eqstring  =  [^']* with escaped chars
	  eqqstring =  [^"]* with escaped chars
   max: 1470 BYTES / Packet
   
2) retrive/delete values
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



/* copy characters from buf[p..] to out until one of *delim is found
   returns: last parsed character
   out will be a zero terminated string
   parsed word will be appended to out
*/
int p_word(int out, int buf, int *p, char *delim )
{
        int ch=0;
	m_clear(out);
        while( *p < m_len(buf) && isspace(ch=CHAR(buf,*p)) ) { (*p)++; }
        while( *p < m_len(buf) && !strchr( delim, ch=CHAR(buf,*p) ) && ch ) { m_putc(out,ch); (*p)++; }
        m_putc(out,0);
        return ch;
}


#define BUF_SIZE 1000

struct keystore {
	int key,val;
};

struct host_db {
	int host, keystore;
};

static int HOSTDB = 0;

int binsert_int(int buf, int key)
{
	void *obj = calloc(1,m_width(buf));
	*(int*)obj = key;
	int ret = m_binsert(buf,obj,cmp_int,0);
	free(obj);
	return ret;
}

int bsearch_int(int buf, int key)
{
	return m_bsearch(&key,buf,cmp_int);
}


void new_host(void *ent, void *unused )
{
	(void) unused;
	struct host_db *h = ent;
	h->keystore = m_create(2,sizeof(struct host_db));	
}

/* return ptr to array-entry that matches `key`, insert `key` if not found and
   call the new() function if defined.
   return: ptr to array-entry
*/
void *m_lookup_int(int buf, int key, void (*new)(void *, void *), void *ctx )
{
	void *obj = calloc(1,m_width(buf));
	*(int*)obj = key;
	int p = m_binsert(buf,obj,cmp_int,0);
	free(obj);
	if( p < 0 ) { /* entry exists */
		return  mls( buf, (-p)-1 );
	}
	if( new ) new( mls( buf,p ), ctx );
	return mls( buf,p );
}

int get_host(int tmp)
{
	if(!HOSTDB) { HOSTDB=m_create(10,sizeof(struct host_db)); }
	int cs = s_mstr(tmp); /* store hostname as const */
	struct host_db *ent = m_lookup_int( HOSTDB, cs, new_host, NULL );
	return ent->keystore;
}
	
int store_keys(int keys, int k, int v)
{
	int cs = s_mstr(k); /* store key as constant */
	struct keystore *ent = m_lookup_int(keys,cs,NULL,NULL);
	ent->val = m_slice(ent->val,0, v,0,-1); /* overwrite val */
	return 0;
}

int reply_to(int sfd, int host)
{
	TRACE(1,"");
	int buf = m_create(100,1);
	int p = bsearch_int( HOSTDB, s_mstr(host) );
	if(p<0) {
		s_printf(buf,0,"<EMPTY>\n" );
		return buf;
	}
	int cnt=0;
	struct host_db *hh=mls(HOSTDB,p);
	struct keystore *key;
	m_foreach(hh->keystore,p,key) {
		s_printf(buf,cnt,"%s=%s ", m_str(key->key), m_str(key->val) );
		cnt = m_len(buf)-1; 
	}
	return buf;
}

void cleanup(void)	
{
	int p1,p2;
	struct keystore *key;
	struct host_db *hh;
	m_foreach(HOSTDB,p1,hh) {
		m_foreach(hh->keystore,p2,key) {
			m_free(key->val);
		}
		m_free(hh->keystore);		
	}
	m_free(HOSTDB);
	HOSTDB=0;
}


static int msg_client( int sfd, char *host, int buf )
{
	int ret = 0;
	if( ! host ) { return ret; }

	int tmp1 = m_create(100,1);
	int tmp2 = m_create(100,1);

	/* get first param */
	int pos = 0;
	p_word( tmp1, buf, &pos, " \t" );
	int p = pos;
	if( m_len(tmp1) < 2 ) goto fin;
		
	/* opt. get second param */
	int ch = p_word( tmp2, buf, &pos, " \t=*" );
	if( m_len(tmp2) < 2 ) {
		if( ch == '*' ) ret = reply_to(sfd,tmp1);
		goto fin;
	}
	if( ch != '=' ) goto fin;
	
	/* we have at least two parameters, lets reset and try to parse key=value pairs */
	/* start at p */

	int keys = 0;
	int k,v;
	k = m_create(10,1);
	v = m_create(10,1);	
	while(1) {
		ch = p_word( k, buf, &p, "=" );	
		if(ch!='=') break;
		p++; /* skip delimeter */			
		ch = p_word( v, buf, &p, " \t" );
		if( m_len(v) < 2 ) break;
		if( !keys ) { keys = get_host(tmp1); }
		store_keys(keys,k,v);
	}
	m_free(k);
	m_free(v);
	ret = s_printf(0,0, "<OK>\n");
	
	fin:
	m_free(tmp1);
	m_free(tmp2);
	return ret;
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
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	ssize_t nread;


	#if 0
	m_init();
	conststr_init();
	m_register_printf();
	trace_level=1;
	int buf = s_printf(0,0, "t1 a=7" );
	msg_client(1,"",buf);
	s_printf(buf,0, "t1 a=7 b=9" );
	msg_client(1,"",buf);
	s_printf(buf,0, "t1 *" );
	msg_client(1,"",buf);
	cleanup();
	conststr_free();
	m_destruct();
	
	exit(0);
#endif
	
	

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	m_init();
	conststr_init();
	m_register_printf();
	trace_level=1;

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

	   int hbuf = m_create(BUF_SIZE,1);
           for (;;) {
               peer_addr_len = sizeof(struct sockaddr_storage);
               if( wait_for_udp( sfd ) != 0 ||
                   (nread=recvfrom(sfd, m_buf(hbuf), m_bufsize(hbuf), 0,
                                    (struct sockaddr *) &peer_addr, &peer_addr_len)) <= 0 )
                 {
                   continue;
                 }
	       m_setlen(hbuf,nread);	       
	       
               char host[NI_MAXHOST], service[NI_MAXSERV];
	       s = getnameinfo((struct sockaddr *) &peer_addr,
                               peer_addr_len, host, NI_MAXHOST,
                               service, NI_MAXSERV, NI_NUMERICSERV);
               if (s == 0) {
		       printf("Received %zd bytes from %s:%s\n",
			      nread, host, service);
               }
               else
                   fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

	       int reply = msg_client(sfd,host,hbuf);
	       if( reply > 0 ) {
		       sendto(sfd, m_buf(reply), m_len(reply), 0, 
			      (struct sockaddr *) &peer_addr, peer_addr_len);
		       m_free(reply);
	       }
           }
	   
	   cleanup();
	   conststr_free();
	   m_destruct();
}
