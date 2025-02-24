#ifndef COMPTAG
#define COMPTAG "Unknown Compiler Options"
#endif
const char *Version = "SQL : PROJECT / " COMPTAG;


#include "m_tool.h"
#include <stdint.h>
#include <time.h>
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"


/* syntax

HOST: hostname
database: database name
database table

[ empty line ]
*/


/* database
   create table bktables (
   host varchar(80), database varchar(40), tablename varchar(80),
       PRIMARY KEY ( host,database,tablename) );

 */

void
store(int h, int d, int t)
{	
	printf("replace into bktables values( '%M','%M', '%M' );\n", h, d, t );
}


void
read_output(FILE *fp)
{
	int res = m_create(1, sizeof(char *));
	int ln = m_create(100, 1);

	int hostname =0;
	int database =0;
	int tablename =0;

	while (s_readln(ln, fp) != -1) {
		if (m_len(ln) == 1) {
			continue;	
		}


		if (s_regex(res, "HOST: (.*)", ln) == 2) {
			hostname = s_cstr(STR(res, 1));
			continue;
		}
			
		if (s_regex(res, "Database: (.*)", ln) == 2) {
			database = s_cstr(STR(res, 1));
			store(hostname, database, 0);
			continue;
		}

		if (s_regex(res, "^(.*)\t *([^ ]*)", ln) == 3 ) {
			database = s_cstr(STR(res, 1));
			tablename = s_cstr(STR(res, 2));
			store(hostname, database, tablename);
			continue;
		}

		fprintf(stderr, "WARN unknown line: %M", ln );
		
	}	

	m_free_strings(res, 0);
	m_free(ln);
}

void
test_inventory(char *fn)
{
	FILE *fp = fopen(fn, "r");
	if (!fp) {
		WARN("%s not found",fn);
		return;
	}
	read_output(fp);
	fclose(fp);
}

int
main(int argc, char **argv)
{
	// printf("Version: %s\n", Version);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 0;
	printf("truncate table bktables;\n");
	test_inventory(argv[1]);
	conststr_free();
	m_destruct();
	return 0;
}
