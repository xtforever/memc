/**
 * UDP server
 */
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"

/* lookup_obj = create(v,ctx) */

#include "m_tool.h"
#include "mls.h"
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t CTRL_C = 0;
void
sigint_handler(int signum)
{
	CTRL_C = 1;
}

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
int
p_word(int out, int buf, int *p, char *delim)
{
	int escaped = 0;
	int in_quote = 0;
	int ch = 0;
	m_clear(out);
	while (*p < m_len(buf) && isspace(ch = CHAR(buf, *p))) {
		(*p)++;
	}
	while (*p < m_len(buf)) {
		ch = CHAR(buf, *p);
		if (ch == '\\' && !escaped) {
			escaped = 1;
		} else if (ch == '"' && !escaped) {
			in_quote = !in_quote;
		} else if (!in_quote && strchr(delim, ch) && !escaped) {
			break;
		} else {
			m_putc(out, ch);
			escaped = 0;
		}
		(*p)++;
	}
	m_putc(out, 0);
	return ch;
}

#define BUF_SIZE 1000

struct keystore {
	int key, val;
};

struct host_db {
	int host, keystore;
};

static int HOSTDB = 0;

/* if a new host is added we need to initialize it */
void
new_host(void *ent, void *unused)
{
	(void)unused;
	struct host_db *h = ent;
	h->keystore = m_create(2, sizeof(struct host_db));
}

/* using generic lookup and host entry create funtion */
int
get_host(int tmp)
{
	if (!HOSTDB) {
		HOSTDB = m_create(10, sizeof(struct host_db));
	}
	int cs = s_mstr(tmp); /* store hostname as const */
	struct host_db *ent = m_blookup_int_p(HOSTDB, cs, new_host, NULL);
	return ent->keystore;
}

/* using generic lookup but no initializer */
int
store_keys(int keys, int k, int v)
{
	int cs = s_mstr(k); /* store key as constant */
	struct keystore *ent = m_blookup_int_p(keys, cs, NULL, NULL);
	ent->val = m_slice(ent->val, 0, v, 0, -1); /* overwrite or create val */
	return 0;
}

void
reply_to(int host, int reply)
{
	TRACE(1, "");
	int p = m_bsearch_int(HOSTDB, s_mstr(host));
	if (p < 0) {
		m_puti(reply,s_strdup_c("<OK>\n"));
		return;
	}
	struct host_db *hh = mls(HOSTDB, p);
	struct keystore *key;
	m_foreach(hh->keystore, p, key)
	{
		int str = s_printf(0,0, "%s=\"%s\"\n", m_str(key->key),
				   m_str(key->val));
		m_puti(reply,str);
	}
	return;
}

void
cleanup(void)
{
	int p1, p2;
	struct keystore *key;
	struct host_db *hh;
	m_foreach(HOSTDB, p1, hh)
	{
		m_foreach(hh->keystore, p2, key) { m_free(key->val); }
		m_free(hh->keystore);
	}
	m_free(HOSTDB);
	HOSTDB = 0;
}

static int
msg_client(int buf, int reply)
{
	int ret = 0;
	int tmp1 = m_create(100, 1);
	int tmp2 = m_create(100, 1);

	/* get first param */
	int pos = 0;
	p_word(tmp1, buf, &pos, " \t\n\r");
	int p = pos;
	if (m_len(tmp1) < 2) {
		WARN("error parsing hostname");
		goto fin;
	}

	/* opt. get second param */
	int ch = p_word(tmp2, buf, &pos, " \t\n\r=*");
	if (m_len(tmp2) < 2) {
		if (ch == '*')
			reply_to(tmp1, reply);
		else
			WARN("error parsing 2nd parameter");
		goto fin;
	}
	if (ch != '=') {
		WARN("error 2nd parameter not k=v");
		goto fin;
	}

	/* we have at least two parameters, lets reset and try to parse
	 * key=value pairs */
	/* start at p */
	int keys = 0;
	int k, v;
	k = m_create(10, 1);
	v = m_create(10, 1);
	while (p < m_len(buf)) {
		ch = p_word(k, buf, &p, "=");
		if (ch != '=')
			break;
		p++; /* skip delimeter */
		ch = p_word(v, buf, &p, " \t\n\r");
		if (m_len(v) < 2) /* need more than a zero */
			break;
		if (!keys) {
			keys = get_host(tmp1);
		}
		store_keys(keys, k, v);
		p++; /* skip delimeter */
	}
	m_free(k);
	m_free(v);
	m_puti(reply,s_strdup_c("<OK>\n"));
fin:
	m_free(tmp1);
	m_free(tmp2);
	return ret;
}

/* returns 0 on success, -1 on timeout or error */
int
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
bind_to(const char *hostname, struct addrinfo *hints)
{
	struct addrinfo *result, *rp;
	int sfd;
	int s = getaddrinfo(NULL, hostname, hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully bind(2).
	   If socket(2) (or bind(2)) fails, we (close the socket
	   and) try the next address. */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break; /* Success */

		close(sfd);
	}
	freeaddrinfo(result); /* No longer needed */

	if (rp == NULL) { /* No address succeeded */
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	return sfd; // success
};

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	int sfd;
	ssize_t nread;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len = sizeof(peer_addr);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	signal(SIGINT, sigint_handler);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 1;
	int reply = m_create(50, sizeof(int));
	int hbuf = m_create(BUF_SIZE, 1);
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
		.ai_socktype = SOCK_DGRAM, /* Datagram socket */
		.ai_flags = AI_PASSIVE,    /* For wildcard IP address */
		.ai_protocol = 0,          /* Any protocol */
	};

	if ((sfd = bind_to(argv[1], &hints)) < 0) {
		ret = EXIT_FAILURE;
		goto cleanup;
	};

	/* Read datagrams and reply to sender */
	for (; !CTRL_C;) {
		if( wait_for_udp(sfd) ) continue;
		nread = recvfrom(sfd, m_buf(hbuf), m_bufsize(hbuf), 0,
				 (struct sockaddr *)&peer_addr,
				 &peer_addr_len);
		if( nread <=0 ) continue;
		m_setlen(hbuf, nread);
		msg_client(hbuf, reply);
		/* send reply, using peer_addr */
		m_clear(hbuf);
		m_puti(reply,s_strdup_c("\n"));
		int p=0,lns = m_len(reply);		
		int d=INT(reply,0);
		while( p<lns ) {
			p++;
			m_slice( hbuf, m_len(hbuf), d, 0, -2 );
			// kein weiterer oder der naechste passt nicht dann absenden
			if( p >= lns || (m_len(d=INT(reply,p))+m_len(hbuf)>=BUF_SIZE))  {
				int err = sendto(sfd, m_buf(hbuf), m_len(hbuf), 0,
				       (struct sockaddr *)&peer_addr, peer_addr_len);
				if( err < 0 ) perror("error sending reply");
				m_clear(hbuf);
			}
		}
		m_clear_list(reply);
	}
	close(sfd);

cleanup:
	m_free(hbuf);
	m_free_list(reply);
	cleanup();
	conststr_free();
	m_destruct();
	return ret;
}
