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

static FILE *safe_fopen(const char *name, const char *acc)
{
	FILE *fp = fopen(name,acc);
	if(!fp) {
		perror("");
		WARN("%s not found", name );
		return NULL;
	}
	return fp;
}

static int skip_until(FILE *fp, const char *name )
{
	int ln = m_create(100,1);
	int ret = -1;
	while( ret && s_readln(ln, fp) == 0 ) 
		ret = m_strcmp_c( ln, name );
	m_free(ln);
	returnr ret;		
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

typedef struct database_st {
	int used;
	MYSQL *conn;
} database_t;

static int DATABASE = 0;
static int MYSQL_CREDS = 0;

int mysql_open_database( const char *name )
{
	if(!MYSQL_CREDS) MYSQL_CREDS = read_creds();
	int h = ctx_init( &DATABASE, 2, sizeof(database_t));
	database_t *db = mls( DATABASE, h );
	return h;
}

static void mysql_closex(int ctx, int p)
{
	database_t *db = mls(ctx,p);	
}

void mysql_close_all(void)
{
	ctx_destruct( &DATABASE, mysql_closex );
}


int main(int argc, char **argv)
{
	m_init();
	mvar_init();
	
	int db = mysql_open_database("custsrv");
	mysql_close_all();

	mvar_destruct();
	m_destruct();
}
