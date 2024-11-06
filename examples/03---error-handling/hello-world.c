#include "mls.h"
#include <stdio.h>
#include <sys/wait.h>
#define EXEC(a) do { puts("\nStarting: " #a "()\n" ); if(!fork()) a(); wait(NULL); puts("...\n"); } while(0)

int create_string(void)
{
	int mystring = s_printf(0,0, "%s", "Hello World" );
	return mystring;
}

void delete_string(int str)
{
	m_free(str);
}



void error_double_free(void)
{
	int mystring = create_string();
	delete_string(mystring);	
	m_free(mystring);
}

void error_out_of_bounds(void)
{
	int mystring = create_string();
	*(char*)mls(mystring,30) = 'x';
	m_free(mystring);
}

void error_handle(void)
{
	int wrong = 264673;
	*(char*)mls(wrong,30) = 'x';	
}



int main( int argc, char **argv )
{
	m_init();
	trace_level=1;
	
	EXEC( error_double_free );
	EXEC( error_out_of_bounds );
	EXEC( error_handle );
	
	m_destruct();	
}
