#ifndef COMPTAG
#define COMPTAG "Unknown Compiler Options"
#endif
const char *Version = "HASHMAP : MEMC DEMO PROJECT / " COMPTAG;

#include "mls.h"
#include "m_tool.h"
#include "m_mysql.h"
#include <stdint.h>
#include <time.h>

void demo_sql(void)
{
	MYSQL *d = m_mysql_connect("localhost","custsrv","jens","linux" );
	int res = m_create(10,sizeof(int));
	int cmd = m_str_from_file( "create-demo.sql" );
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
}


	

int
main()
{
	printf("Version: %s\n", Version);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 1;

	printf("SQL Demonstration\n");
	demo_sql();
	
	conststr_free();
	m_destruct();
	return 0;
}
