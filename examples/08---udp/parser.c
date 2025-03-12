#include <stdio.h>
#include "mls.h"

int p_word(int out, int buf, int *p, char *delim )
{
	int escaped = 0;
	int in_quote = 0;
        int ch=0;
	m_clear(out);
        while( *p < m_len(buf) && isspace(ch=CHAR(buf,*p)) ) { (*p)++; }
	while( *p < m_len(buf) ) {
		ch=CHAR(buf,*p);	      
		if (ch == '\\' && !escaped) {
			escaped = 1;
		} else if ( ch == '"' && !escaped) {
			in_quote = !in_quote;
		} else if (!in_quote && strchr(delim,ch) && !escaped) {
			break;
		} else {
			m_putc(out,ch);
			escaped = 0;
		}
		(*p)++;
	}
	m_putc(out,0);
        return ch;
}

void test1(const char  *a, const char *res )
{
	int buf = s_printf(0,0, "%s", a );
	int out = m_create(10,1);
	int p = 0 ;
	p_word( out, buf, &p, " " );
	if( strcmp( m_buf(out), res ) ) 
		printf("error _%s_ != _%s_\n", m_str(out), res );

	m_free(buf);m_free(out);
	
}
	

int main()
{
	m_init();
	test1( "hello ", "hello" );
	test1( " hello ", "hello" );
	test1( " \" hello\" ", " hello" );
	test1( " \\\"abc  ", "\"abc" );
	m_destruct();
}
