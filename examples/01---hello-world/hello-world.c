#include "mls.h"
#include <stdio.h>

int main( int argc, char **argv )
{
	m_init();
	trace_level=1;
	
	int mystring = s_printf(0,0, "%s", "Hello World" );

	write(1, m_buf(mystring), m_len(mystring) );
	m_free(mystring);

	m_destruct();	
}
