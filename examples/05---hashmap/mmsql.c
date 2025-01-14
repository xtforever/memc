#ifndef COMPTAG
#define COMPTAG "Unknown Compiler Options"
#endif
const char *Version = "MMSQL : MEMC DEMO PROJECT / " COMPTAG;

#include "m_csv.h"
#include "m_tool.h"
#include "m_mysql.h"
#include <stdint.h>
#include <time.h>
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"


#define CONF "create-demo.conf"
#define CSV "demo.csv"

int m_mysql_exec_c( MYSQL *conn, char *s )
{
	int res = v_init();
	int ret = m_mysql_query( conn, s, res );
	v_free(res);
	return ret;
}

int m_mysql_exec( MYSQL *conn, int mstr )
{
	return m_mysql_exec_c(conn,m_str(mstr));	
}


void demo_sql(void)
{
	/* fetch field names for mysql that corresponds to the demo.csv header */
	int cmd = csv_reader( CONF );
	if( m_len(cmd) < 1 ) ERR("%s not found", CONF );
	int fld = s_implode(0, INT(cmd,0), s_cstr(","));
	MYSQL *d = m_mysql_connect("localhost","custsrv","jens","linux" );
	if(!d) ERR("could not connect to mysql");
	m_mysql_exec_c(d,  "drop table if exists mcsvdemo" );
	m_mysql_exec(d, cs_printf("create table mcsvdemo ( %M )", fld ));
	
	FILE *fp = fopen(CSV, "r");
	if (!fp)
		ERR("file %s not found", CSV);


	int p,*j;
	int pref = s_printf(0,0,"insert into mcsvdemo values ( ");
	int len = m_len(pref) -1;
	int ln = m_create(100, 1);
	int cnt=0;
	m_fscan(0, 10, fp); /* skip first */
	while (m_fscan(ln, 10, fp) != -1) {
		cnt++;
		int w = csv_split(ln);
		m_clear(ln);
		m_setlen(pref,len);
		m_foreach(w,p,j) {
			if( p ) m_putc(pref,',');
			m_putc(pref,'\'');
			escape_buf( pref, m_str(*j) );
			m_putc(pref,'\'');			 
		}
		m_putc(pref,')');
		m_putc(pref,0);
		m_free_list(w);
		if( m_mysql_exec(d,pref) < 0 ) {
			WARN("Problem with Line %d", cnt );
		}
		// printf("%M\n", pref );
	}
	
	m_free(ln);
	m_free(pref);
	m_mysql_close(d);


	m_free(fld);
	csv_free(cmd);
	return;

	/*
	int res = m_create(10,sizeof(int));
	cmd = m_str_from_file( "create-demo.sql" );
	if( m_len(cmd) < 1 ) ERR("create-demo.sql must be created");
	int p,*ln;
	m_foreach(cmd,p,ln) {
		int err = m_mysql_query( d, m_buf(*ln), res );
		if(err < 0 ) printf("create-demo.sql error in line %d\n", p+1 );
	}
	v_free(res);	
	m_free_list(cmd);


	// insert into democsv values ( ) 


	m_mysql_close(d);
	*/
}


	

int
main()
{
	printf("Version: %s\n", Version);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 0;

	printf("SQL Demonstration\n");
	demo_sql();
	
	conststr_free();
	m_destruct();
	return 0;
}
