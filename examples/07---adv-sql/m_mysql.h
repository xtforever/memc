#ifndef M_MYSQL_H
#define M_MYSQL_H
#include <mysql/mysql.h>

MYSQL *m_mysql_connect( char *host, char *database, char *user, char *pwd );
void m_mysql_close( MYSQL *conn);
int m_mysql_query( MYSQL *conn, char *query, int res );

char *str_expand( int vars, char *s);
int m_mysql_create_insert_query(int buffer, char *tablename, int values, int row );


void table_dump( MYSQL *conn, char *tablename );
int get_field_by_index(int res, int fieldnum );
char * get_entry( int res, int x, int y );
int field_count(int res);
int row_count(int res);
char *field_name(int res, int i);
int mysql_quote_escape(int b, int s);

#endif
