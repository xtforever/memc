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

   check_mk_agent/local:
   local-check-key-not-number

   check_mk_agent/local/number:
   local-check-with-timer

   plugins/
   plugin-not-number

   /usr/lib/check_mk_agent/plugins/14400:
   plugin-with-timer

 */


/* database

   hostname, local/plugin, cache, name

 */

void
store(int h, int p, int cache, int n)
{
	printf("insert into cmk values( '%M', %d, %d, '%M' );\n", h, p, cache,
	       n);
}


void
read_output(FILE *fp)
{
	int res = m_create(1, sizeof(char *));
	int ln = m_create(100, 1);
	int state = 0;
	int cache;
	int hostname;

	while (s_readln(ln, fp) != -1) {
		if (m_len(ln) == 1) {
			state = 0;
			continue;
		}

		switch (state) {
		case 0:
			if (s_regex(res, "Output from (.*)", ln) == 2) {
				hostname = s_cstr(STR(res, 1));
				continue;
			}
			if (s_regex(res, "local:", ln)) {
				state = 1;
				continue;
			}
			if (s_regex(res, "local/([0-9]*)", ln == 2)) {
				cache = atoi(STR(res, 1));
				state = 2;
				continue;
			}
			if (s_regex(res, "plugins:", ln)) {
				state = 3;
				continue;
			}
			if (s_regex(res, "plugins/([0-9]*)", ln) == 2) {
				cache = atoi(STR(res, 1));
				state = 4;
				continue;
			}
			break;
		case 1: /* local */
			if (s_regex(res, "^[0-9]*$", ln)) {
				continue;
			}
			store(hostname, 0, 0, ln);
			break;
		case 2: /* local/cache */
			store(hostname, 0, cache, ln);
			break;
		case 3: /* plugin */
			if (s_regex(res, "^[0-9]*$", ln)) {
				continue;
			}
			store(hostname, 1, 0, ln);
			break;
		case 4: /* plugin cache */
			store(hostname, 1, cache, ln);
			break;
		}
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
	test_inventory(argv[1]);
	conststr_free();
	m_destruct();
	return 0;
}
