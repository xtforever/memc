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

void error_use_after_free(void)
{
	// create a buffer and store a dummy
	int old_buffer = m_create(100,sizeof(int));
	m_puti(old_buffer,50);
	// delete buffer and create a new one
	// the new buffer will reuse the old handle
	// but the handle will get an
	// use-afer-free protection bitmask
	m_free(old_buffer);
	int new_buffer = m_create(100,sizeof(int));
	printf("Handles: old_buffer=%d, new_buffer=%d\n",
	       old_buffer & 0xffffff,
	       new_buffer & 0xffffff );
	printf("Real Handles: old_buffer=%d, new_buffer=%d\n",
	       old_buffer, new_buffer );
	m_puti(old_buffer,51);
}


int main( int argc, char **argv )
{
	m_init();
	trace_level=1;
	
	EXEC( error_double_free );
	EXEC( error_out_of_bounds );
	EXEC( error_handle );
	EXEC( error_use_after_free );
	
	m_destruct();	
}
