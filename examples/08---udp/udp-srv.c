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
#include <time.h>

const char *default_db = "db.dat";

volatile sig_atomic_t CTRL_C = 0;
void
sigint_handler(int signum)
{
	CTRL_C = 1;
	WARN("sigint received");
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
   leading spaces are ignored
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
	time_t stamp;
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
	h->keystore = m_create(2, sizeof(struct keystore));
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
	ent->stamp = time(NULL);
	return 0;
}

int
reply_to(int host, int buf)
{
	TRACE(1, "lookup host %M", host );
	int p = m_bsearch_int(HOSTDB, s_mstr(host));
	if (p < 0) {
		s_printf(buf,0, "ERR\nNo Keys found for %M\n", host );
		return buf;
	}
	struct host_db *hh = mls(HOSTDB, p);
	struct keystore *ks;
	s_printf(buf, 0, "OK\n" );
	m_foreach(hh->keystore, p, ks)
	{
		s_printf(buf, -1,  "%M=%M\n", ks->key, ks->val );
	}
	return buf;
}

int
reply_single(int host, int key, int buf)
{
	TRACE(1, "");
	int p = m_bsearch_int(HOSTDB, s_mstr(host));
	if (p < 0) {
		s_printf(buf,0, "ERR\nNo Keys found for %M\n", host );
		return buf;
	}
	struct host_db *hh = mls(HOSTDB, p);
	p = m_bsearch_int(hh->keystore, s_mstr(key));
	if (p < 0) {
		s_printf(buf,0, "ERR\nKEY %M NOT FOUND\n", key);
		return buf;
	}
	struct keystore *ks = mls(hh->keystore, p);
	s_printf(buf, 0, "OK\n%M\n", ks->val );
	return buf;
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

void save_database( int fn  )
{
	const char *name = s_isempty(fn) ? default_db : m_str(fn);	
	FILE *fp=fopen( name, "wb" );
	if(!fp) ERR("could not create %s", name );
	int p1, p2;
	struct keystore *key;
	struct host_db *hh;
	m_foreach(HOSTDB, p1, hh) {
		m_foreach(hh->keystore, p2, key) {
			fprintf(fp,"%M %ld %M \"%M\"\n",hh->host, key->stamp, key->key,key->val ); 
		}
	}
	fclose(fp);
}

int merge_database(int fn)
{
	char *id = NULL;
	long long number;
	char *code = NULL;
	char *quoted_value = NULL;
	FILE *fp=fopen( m_str(fn), "rb" );
	if(!fp) {
		WARN("could not read %M", fn );
		return -1;
	}
	while( ! feof(fp) ) {
		int r = fscanf(fp, "%ms %lld %ms \"%m[^\"]\"",
			       &id, &number, &code, &quoted_value);
		if( r != 4 ) continue;
		int keys = get_host( s_cstr(id) );
		int cs = s_cstr(code); /* store key as constant */
		TRACE(1,"MERGE KEY '%s' Id=%d", code, cs );
		struct keystore *ent = m_blookup_int_p(keys, cs, NULL, NULL);
		if( number > ent->stamp ) {
			TRACE(1,"UPDATE %M", cs );
			ent->stamp = number;
			m_free(ent->val);
			ent->val = s_strdup_c(quoted_value);
		}		
		free(id);
		free(code);
		free(quoted_value);
	}
	fclose(fp);
	//conststr_stats();
	return 0;
}

int load_database(void)
{
	cleanup();
	HOSTDB = m_create(10, sizeof(struct host_db));
	return merge_database( s_cstr( default_db ) );
}


// syntax:
// host key=value
// host *
// host key
//
static int
msg_client(int buf, int reply)
{
	int ret = 0;
	int tmp1 = m_create(100, 1);
	int tmp2 = m_create(100, 1);
	m_clear(reply);

	/* get first param */
	int pos = 0;
	p_word(tmp1, buf, &pos, " \t\n\r");
	
	int p = pos;
	if (m_len(tmp1) < 2) {
		WARN("error parsing hostname");
		s_strcpy_c(reply, "ERR\nSYNTAX ERROR: hostname not found\n");
		goto fin;
	}

	if( s_strcmp_c(tmp1,0,":SAVE") == 0 ) {
		p_word(tmp2,buf,&pos, "\n" );
		save_database( tmp2 );
		goto fin_ok;
	}
	
	if( s_strcmp_c(tmp1,0,":LOAD") == 0 ) {
		p_word(tmp2,buf,&pos, "\n" );
		if(! merge_database(tmp2) ) goto fin_ok;
		s_printf(reply,0,"ERR\ncan not load '%M'\n",tmp2 );
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
		reply_single(tmp1, tmp2, reply);
		goto fin;
	}

	/* we have at least two parameters,  */
	/* and the secoond ends with '='  */
	/* lets reset and try to parse  */
	/* key=value pairs */
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

fin_ok:
	s_strcpy_c(reply, "OK\n");

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
	int sfd;
	struct sockaddr_storage peer_addr;
	struct addrinfo hints;
	ssize_t nread;
	socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
	int ret = EXIT_SUCCESS;
	signal(SIGINT, sigint_handler);

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
	trace_level = 1;
	load_database();
	
	int reply = m_create(50, 1);
	int hbuf = m_create(BUF_SIZE, 1);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	if ((sfd = bind_to(argv[1], &hints)) < 0) {
		ret = EXIT_FAILURE;
		goto cleanup;
	};

	/* Read datagrams and reply to them back to sender */
	for (; !CTRL_C;) {
		if (wait_for_udp(sfd) != 0
		    || (nread = recvfrom(sfd, m_buf(hbuf), m_bufsize(hbuf), 0,
		                         (struct sockaddr *)&peer_addr,
		                         &peer_addr_len))
		           <= 0) {
			continue;
		}
		m_setlen(hbuf, nread);
		msg_client(hbuf, reply);
		if (m_len(reply)) {
			s_puts(reply);
			sendto(sfd, m_buf(reply), m_len(reply), 0,
			       (struct sockaddr *)&peer_addr, peer_addr_len);
		}
	}
	close(sfd);

cleanup:
	m_free(hbuf);
	m_free(reply);
	cleanup();
	conststr_free();
	m_destruct();
	return ret;
}
