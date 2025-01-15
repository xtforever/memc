#ifndef COMPTAG
#define COMPTAG "Unknown Compiler Options"
#endif
const char *Version = "MMSQL : MEMC DEMO PROJECT / " COMPTAG;

#include "m_csv.h"
#include "m_mysql.h"
#include "m_tool.h"
#include <stdint.h>
#include <time.h>
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"

#define CONF "create-demo.conf"
#define CSV "demo.csv"

int
m_mysql_exec_c(MYSQL *conn, char *s)
{
	int res = v_init();
	int ret = m_mysql_query(conn, s, res);
	v_free(res);
	return ret;
}

int
m_mysql_exec(MYSQL *conn, int mstr)
{
	return m_mysql_exec_c(conn, m_str(mstr));
}

int
m_mysql_select(MYSQL *conn, int req)
{
	int res = v_init();
	int ret = m_mysql_query(conn, m_str(req), res);
	if (ret < 0) {
		v_free(res);
		return -1;
	}
	return res;
}

void
demo_sql(void)
{

	MYSQL *d = m_mysql_connect("localhost", "custsrv", "jens", "linux");
	if (!d)
		ERR("could not connect to mysql");
	goto test_select;

	/* fetch field names for mysql that corresponds to the demo.csv header
	 */
	int cmd = csv_reader(CONF);
	if (m_len(cmd) < 1)
		ERR("%s not found", CONF);
	int fld = s_implode(0, INT(cmd, 0), s_cstr(","));
	m_mysql_exec_c(d, "drop table if exists mcsvdemo");
	m_mysql_exec(d, cs_printf("create table mcsvdemo ( %M )", fld));

	FILE *fp = fopen(CSV, "r");
	if (!fp)
		ERR("file %s not found", CSV);

	int p, *j;
	int pref = s_printf(0, 0, "insert into mcsvdemo values ( ");
	int len = m_len(pref) - 1;
	int ln = m_create(100, 1);
	int cnt = 0;
	m_fscan(0, 10, fp); /* skip first */
	while (m_fscan(ln, 10, fp) != -1) {
		cnt++;
		int w = csv_split(ln);
		m_setlen(pref, len);
		m_foreach(w, p, j)
		{
			if (p)
				m_putc(pref, ',');
			mysql_quote_escape(pref, *j);
		}
		m_putc(pref, ')');
		m_putc(pref, 0);
		m_free_list(w);
		m_clear(ln);
		if (m_mysql_exec(d, pref) < 0) {
			WARN("Problem with Line %d", cnt);
		}
		// printf("%M\n", pref );
	}

	m_free(ln);
	m_free(pref);
	m_free(fld);
	csv_free(cmd);

test_select:
	int req = s_cstr("select FirstName,LastName,Email from mcsvdemo where "
	                 "Email like 'e%' limit 3");
	int ret = m_mysql_select(d, req);
	if (ret <= 0)
		ERR("query error");
	int result = ret;
	int nrows = row_count(result);
	int ncols = field_count(result);
	for (int y = 0; y < nrows; y++) {
		for (int x = 0; x < ncols; x++) {
			printf("%-15s", get_entry(result, x, y));
		}
		putchar(10);
	}

	m_mysql_close(d);
	v_free(result);
	return;
}

/* syntax

   check_mk_agent/local:
   local-check-key-not-number

   check_mk_agent/local/number:
   local-check-with-timer

   plugins/
   plugin-not-number

   /usr/lib/check_mk_agent/plugins/14400:
   plugin-with-timer

 */

int
m_strncmp(int s0, int p0, int s1, int p1, int len)
{
	int l0 = m_len(s0);
	int l1 = m_len(s1);

	while (len--) {
		if (p0 >= l0)
			return -1;
		if (p1 >= l1)
			return 1;
		int diff = CHAR(s0, p0) - CHAR(s1, p1);
		if (diff)
			return diff;
		p0++;
		p1++;
	}
	return 0;
}

int
EndsWith(int str, int suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = m_len(str);
	size_t lensuffix = m_len(suffix);
	if (lensuffix > lenstr)
		return 0;
	return m_strncmp(str, lenstr - lensuffix, suffix, 0, lensuffix) == 0;
}


/* database

   hostname, local/plugin, cache, name 

 */


void store(int h,int p, int cache, int n)
{
	printf("insert into cmk values( '%M', %d, %d, '%M' );\n",
	       h,p,cache,n);
}


int
readln(int buf, FILE *fp)
{
	m_clear(buf);
	int ret = m_fscan(buf, 10, fp);
	if( ret < 0 && m_len(buf) > 1 ) return 0;
	return ret;
}

int regexmatch(char *regex,int buf)
{
	int res = m_regex(0, regex, m_str(buf));
	int ret = m_len(res);
	m_free_strings(res, 0);
	return ret;
}

void
read_output(FILE *fp)
{
	int res = m_create(1, sizeof(char *));
	int ln = m_create(100, 1);
	// int cnt=0, ch=0;
	int state = 0;
	int cache;
	int hostname;
	// char *regex;
	while (readln(ln, fp) != -1) {

		
		if (m_len(ln) == 1) {
			state = 0;
			continue;
		}

		switch (state) {

		case 0:
			m_regex(res, "Output from (.*)", m_str(ln));
			if (m_len(res) == 2 ) {
				hostname = s_cstr(STR(res,1));
				continue;
			}
			
			m_regex(res, "local:", m_str(ln));
			if (m_len(res)) {
				state = 1;
				continue;
			}
			m_regex(res, "local/([0-9]*)", m_str(ln));
			if (m_len(res) == 2 ) {
				cache = atoi( STR(res,1) );
				state = 2;
				continue;
			}
			m_regex(res, "plugins:", m_str(ln));
			if (m_len(res)) {
				state = 3;
				continue;
			}
			m_regex(res, "plugins/([0-9]*)", m_str(ln));
			if (m_len(res) == 2) {
				cache = atoi( STR(res,1) );
				state = 4;
				continue;
			}
			break;
		case 1: /* local */
			m_regex(res, "^[0-9]*$", m_str(ln));
			if (m_len(res)) {
				continue;
			}
			store(hostname,0,0,ln);
			// printf("%M\n", ln);
			break;
		case 2: /* local/cache */
			store(hostname,0,cache,ln);
			// printf("%M, %d\n", ln, cache);
			break;
		case 3: /* plugin */
			if( regexmatch( "^[0-9]*$", ln ) ) {
				continue;
			}
			store(hostname,1,0,ln);
			// printf("%M\n", ln);
			break;
		case 4: /* plugin cache */
			store(hostname,1,cache,ln);
			// printf("plugin %M, %d\n", ln, cache);
			break;
		}
	}
	m_free_strings(res, 0);
	m_free(ln);
}

void
test_inventory(char *fn)
{
	// char *fn = "output_cust35.fra.4hr.de.txt";
	// char *dir = "/home/jens/tmp/inventory/";
	char *dir = "";
	
	int name = s_app(0, dir, fn, NULL);
	FILE *fp = fopen(m_str(name), "r");
	if (!fp) {		
		WARN("%M not found",name );
		m_free(name);
		return;
	}
	read_output(fp);
	fclose(fp);
	m_free(name);
}

int
main(int argc, char **argv)
{
	// printf("Version: %s\n", Version);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 0;

	// printf("SQL Demonstration\n");
	// demo_sql();
	test_inventory(argv[1]);
	conststr_free();
	m_destruct();
	return 0;
}
