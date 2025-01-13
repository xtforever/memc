#include "mls.h"
#include <stdio.h>
#include <stdarg.h>
#include <mysql/mysql.h>
// #include <mysql/my_global.h>
#include "m_mysql.h"
/* TABLE:

VARSET table;

tablename               sql table name
fields                  list of fields

fieldname[0]            first column
fieldname[1]            2nd column
...
*/

/*
int verbose = 0;
void vlog( const char *format, ... )
{
  va_list argptr;
  if( verbose ) {
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
  }
}
*/

static void m_puts( int query, char *s )
{
  m_write(query, m_len(query), s, strlen(s) );
}


MYSQL *m_mysql_connect( char *host, char *database, char *user, char *pwd )
{
  MYSQL *conn;

  TRACE(3,"MySQL client version: %s\n", mysql_get_client_info());


  if(! (conn = mysql_init(NULL))) goto error;
  if(! (mysql_real_connect(conn, host, user, pwd,
                           database, 0, NULL, 0))) goto error;
  goto end_of_function; // keep compiler happy


 error:
  ERR("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));

 end_of_function:
  return conn;
}

void m_mysql_close( MYSQL *conn)
{
  mysql_close(conn);
}


/**
   returns
   ERROR <0:
   -1 : Connection error
   -2 : result set read error
   OK >=0:
   1 : No Result Set
   0 : OK, result set stored in varset res
*/
int m_mysql_query( MYSQL *conn, char *query, int res )
{
  char *s;
  MYSQL_RES *result;
  MYSQL_ROW row;
  MYSQL_FIELD *fields;
  int num_fields;
  int i;

  TRACE(2,query);
  if( mysql_query(conn, query ) ) {
    WARN("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
    return -1;
  }

  if(mysql_field_count(conn) == 0 || res < 1 )
    {
      return 1;
    }

  if( res < 1 ) return 1;

  result = mysql_store_result(conn);
  if( !result )
    ERR("Error: %s\n", mysql_error(conn));

  num_fields = mysql_num_fields(result);
  fields = mysql_fetch_fields(result);
  for(i = 0; i < num_fields; i++)
    {
      s = fields[i].name;
      TRACE(1,"Field %u is %s\n", i, s);
      v_clr(res,s);
      v_set(res,"_RESFIELDS", s, i+1 );
    }

  while ((row = mysql_fetch_row(result)))
    {
      for(i = 0; i < num_fields; i++)
      {
        char *s = row[i] ? row[i] : "";
        TRACE(1,"%s ", s );
        v_set( res, fields[i].name, s, VAR_APPEND );
      }
    }

  mysql_free_result(result);
  return 0;
}


/** do not free the returned string
 */
char *str_expand(int vars, char *s)
{
    return se_string(vars,s);
}

/**
   @Brief Insert a single row of Data into a SQL Table.
   using the following statement, this function will
   write some values to a sql table:

   INSERT INTO table (a,b,c) VALUES (1,2,3)
   ON DUPLICATE KEY UPDATE a=1,b=2,c=3

   connection must be initialized via m_mysql_open

   array values must contain field-names

*/
int m_mysql_create_insert_query( int buffer, char *tablename, int values, int row )
{
  int p, *d;
  int q = buffer ? buffer : m_create(100,1);
  m_puts(q, "INSERT INTO ");
  m_puts(q, tablename );
  m_puts(q, " (" );
  m_foreach( values, p, d ) {
    if( p > 0 ) m_putc( q, ',' );
    m_puts(q, STR(*d,0) );
  }
  m_puts(q, ") VALUES (");

  m_foreach( values, p, d ) {
    if( p > 0 ) m_putc( q, ',' );
    m_putc(q, '\'' );
    m_puts(q, STR(*d,1+row) );
    m_putc(q, '\'' );
  }

  m_puts( q, ") ON DUPLICATE KEY UPDATE ");
  m_foreach( values, p, d ) {
    if( p > 0 ) m_putc( q, ',' );
    m_puts(q, STR(*d,0) );
    m_putc(q, '=');
    m_putc(q, '\'' );
    m_puts(q, STR(*d,1+row) );
    m_putc(q, '\'' );
  }

  m_putc(q, 0 );
  return q;
}


void table_dump( MYSQL *mysql, char *tablename )
{
  int i, nrows;
  int crsr;
  int p;
  char **strp;
  int width;
  int tabs = m_create(10,sizeof(int));
  int var_list_of_fields;
  int var_field;
  char *var_name;
  int e;
  char *query;
  int result = v_init(); // this time it is usefull to initialize result

  asprintf( &query, "select * from %s", tablename );
  e=m_mysql_query( mysql, query, result );
  if( e < 0 ) goto exit_func;
  if( e == 1 ) {
    printf("query %s returns no results", query );
    goto exit_func;
  }

  //
  // sql_query returns a list of column names in _RESFIELDS
  // "_RESFIELDS" is the first entry in result
  //
  var_list_of_fields = v_lookup( result, "_RESFIELDS" );
  p=0; crsr=0;
  while( m_next(var_list_of_fields, &p, &strp ) ) {
    width = strlen(*strp);
    if( width < 10 ) width = 10;
    if( p > 1 ) printf( " | " );
    printf( "%*s", width, *strp );
    m_put( tabs, &width );
  }
  puts("");

  var_name = STR(var_list_of_fields, 1);
  var_field = v_lookup( result, var_name );
  nrows = m_len( var_field ) -1;

  for(i=0;i<nrows;i++) {
    p=0;
    while( m_next(var_list_of_fields, &p, &strp ) ) {
      if( p > 1 ) printf( " | " );
      width = INT(tabs,i);
      printf(  "%*s", width, v_get( result, *strp, i+1) );
    }
    puts("");
  }

 exit_func:
  v_free(result);
  m_free(tabs);
  free(query);
}

int get_field_by_index(int res, int fieldnum )
{
    char *s = v_get( res, "_RESFIELDS", fieldnum+1 );
    return v_lookup( res, s );
}
char * get_entry( int res, int x, int y )
{
    return v_get( res, v_get(res, "_RESFIELDS", x+1), y+1 );
}
int field_count(int res)
{
    return m_len( v_lookup(res,  "_RESFIELDS" ) ) -1;
}
int row_count(int res)
{
    return m_len( get_field_by_index(res,0))-1;
}
char *field_name(int res, int i)
{
    return v_get( res, "_RESFIELDS", i+1 ); 
}
