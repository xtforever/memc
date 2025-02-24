/*
SCREEN:
BK Tables --+
| DB: $a    |
| T:  $b    |
+-----------+

+ Hosts ------------------------------------------------------
|$h 1            
|   2
|   3
|   4
|   5
|   6
|   7
|   8
|   9
|   0
+------------------------------------------------------------

VARS:
a  4 1      bktables.db_count
b  4 1      bktables.tables_count
h  60 10 

SQL:
bktables: SELECT count(distinc database) AS db_count from bktables where tablename=''
bktables: SELECT host, COUNT(tablename) AS table_count FROM bktables GROUP BY host order by table_count desc limit 10




READ
SCREEN (x,y) / VARS( name,w,h,val )
SQL( name, script )

exec script
write vars to screen
print screen

*/

/*


 */


#include "m_tool.h"
#include "var5.h"
#include "m_mysql.h"
#include "ctx.h"
#include <pwd.h>
#include <stdio.h>
#include <glob.h>
#include <string.h>
#include <errno.h>
#include <wordexp.h>
FILE *safe_fopen(const char *pattern, const char *mode)
{
    wordexp_t p;
    FILE *fp = NULL;
    // Perform tilde expansion on the filename
    if (wordexp(pattern, &p, 0) != 0
	|| (p.we_wordc <= 0)) {
	    wordfree(&p);
	    return NULL;
    }

    fp = fopen(p.we_wordv[0], mode);
    if (fp == NULL) {
            fprintf(stderr, "Failed to open file %s: %s\n", 
		    p.we_wordv[0], strerror(errno));
    }
    wordfree(&p);    
    return fp;
}

#if 0
static int skip_until(FILE *fp, const char *name )
{
	int ln = m_create(100,1);
	int ret = -1;
	while( ret && s_readln(ln, fp) == 0 ) 
		ret = s_strcmp_c( ln, name );
	m_free(ln);
	return ret;		
}

static int  mvar_assign(int mstr )
{
    int ls =  m_split_list( m_str(mstr), "=" );
    if( m_len(ls) != 2 ) goto leave;
    int v = mvar_parse( INT(ls,0), VAR_STRING );
    mvar_put_string(v, m_str(INT(ls,1)), 0);

 leave:
    m_free_list(ls);
    return v;
}


static int read_creds(void)
{
	FILE *fp = safe_fopen("~/.my.cnf", "rt" );
	if(!fp) return -1;
	skip_until( fp, "[client]" );       	
	int ln = m_create(100,1);
	int vars = mvar_vset();
	while( s_readln(ln,fp) == 0 ) {
		if( m_len(ln) < 2 ) continue; /* leere zeile */
		if( CHAR(ln,0) == '[' ) break; /* neue section */
		mvar_assign(ln);	
	};
	m_free(ln);
	return vars;	
}
#endif
\
typedef struct database_st {
	int used;
	MYSQL *conn;
	int vset;
} database_t;

static int DATABASE = 0;


#define MAX_PATH_LEN 1024

char *tilde_expand(const char *path) {
    if (!path || path[0] != '~') {
        return strdup(path);  // Return a copy if no tilde found
    }
    
    char expanded[MAX_PATH_LEN];
    const char *home = NULL;
    
    if (path[1] == '/' || path[1] == '\0') {
        // Case: "~" or "~/something"
        home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
    } else {
        // Case: "~username/..."
        char username[MAX_PATH_LEN];
        const char *slash = strchr(path, '/');
        size_t len = slash ? (size_t)(slash - path - 1) : strlen(path + 1);
        strncpy(username, path + 1, len);
        username[len] = '\0';

        struct passwd *pw = getpwnam(username);
        if (pw) home = pw->pw_dir;
    }
    
    if (home) {
        snprintf(expanded, sizeof(expanded), "%s%s", home, path + (path[1] == '/' ? 1 : strlen(path)));
        return strdup(expanded);
    }
    
    return strdup(path);  // Return original if expansion fails
}

int mysql_open_database( const char *database )
{
	// Use .my.cnf for default connection values

	char *pattern = "~/.my.cnf";
	char *filename = NULL;
		
	MYSQL *conn = mysql_init(NULL);
	if (conn == NULL) {
		fprintf(stderr, "mysql_init() failed\n");
		return -1;
	}
	filename = tilde_expand(pattern);
	// Establish the connection (NULL parameters allow .my.cnf to be used)
	mysql_options(conn, MYSQL_READ_DEFAULT_FILE, filename );
	if (mysql_real_connect(conn, NULL, NULL, NULL, database, 0, NULL, 0) == NULL) {
		fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
		mysql_close(conn);
		free(filename);
		return -1;
	}

	int h = ctx_init( &DATABASE, 2, sizeof(database_t));
	database_t *db = mls( DATABASE, h );
	db->conn = conn;	
	free(filename);
	return h + 1;
}

static void mysql_closex(int *ctx, int p)
{
	database_t *db = mls(*ctx,p);
	mysql_close(db->conn);
}

void mysql_close_all(void)
{
	ctx_destruct( &DATABASE, mysql_closex );
}

int var5_mysql_query( int hdb, int sql, int group )
{
	database_t *db = mls( DATABASE, hdb-1 );
	MYSQL *conn = db->conn;
	char *s;
	MYSQL_RES *result;
	MYSQL_ROW row;
	MYSQL_FIELD *fields;
	int num_fields;
	int i;
	const char *query =  m_str(sql);
	
	TRACE(2, query);
	if (mysql_query(conn, query  )) {
		WARN("Error %u: %s\nQuery:%s\n", mysql_errno(conn),
		     mysql_error(conn), query);
		return -1;
	}

	if (mysql_field_count(conn) == 0 || group < 0) {
		return 1;
	}

	result = mysql_store_result(conn);
	if (!result)
		ERR("Error: %s\n", mysql_error(conn));

	num_fields = mysql_num_fields(result);
	fields = mysql_fetch_fields(result);

	int var_res = mvar_lookup( group, "_RESFIELDS", VAR_STRING ); 
	mvar_clear(var_res);
	int field_var = m_create( num_fields, sizeof(int));
	
	for (i = 0; i < num_fields; i++) {
		s = fields[i].name;
		TRACE(1, "Field %u is %s\n", i, s);
		mvar_put_string( var_res, s, i );
		int var = mvar_lookup( group, s, VAR_STRING );
		mvar_clear(var);
		m_puti(field_var,var); 
	}

	while ((row = mysql_fetch_row(result))) {
		for (i = 0; i < num_fields; i++) {
			char *s = row[i] ? row[i] : "";
			TRACE(1, "VALUE%d=%s ",i, s);
			int v = INT(field_var,i);
			mvar_put_string(v, s, VAR_APPEND);
		}
	}

	m_free(field_var);
	mysql_free_result(result);
	return 0;
}


int main(int argc, char **argv)
{
	m_init();
	mvar_init();
	conststr_init();
	trace_level=1;
	int db = mysql_open_database("custsrv");

	const char *sql ="SELECT count(distinct database) AS db_count from bktables where tablename=''";
	int group = mvar_vset();
	var5_mysql_query(db, s_cstr(sql), group );
	
	mysql_close_all();

	mvar_destruct();
	conststr_free();
	m_destruct();
}
