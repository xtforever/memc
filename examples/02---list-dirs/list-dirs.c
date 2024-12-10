#include "mls.h"
#include <stdio.h>
#include <glob.h>

#define TRACE_ALL 2
#define TRACE_FN  3


/** append cstr 's' to string list 'm'
 * create an array of bytes, copy the string to the new array, append handle of that array to array 'm'
 **/
void copy_from_cstr(int m, const char *s)
{
	TRACE(TRACE_FN, "" );
	if( s==0 || *s==0 ) {
		int w=m_create(1,1);
		m_putc(w,0);
		m_put(m,&w);
		return;
	}
	
	int len=strlen(s)+1;
	int w=m_create(len,1);
	m_write(w,0,s,len);
	m_put(m,&w);
}

/* check if array 'd' could be a string and copy the bytes to 'stdout' then output a newline */
void put_string(int d)
{
	TRACE(TRACE_FN, "" );
	if( d && m_len(d) && m_width(d)==1 && CHAR(d, m_len(d)-1) == 0 ) {
		fwrite( m_buf(d), m_width(d), m_len(d)-1, stdout );
	}
	fputc(10, stdout );
}

int compar(const void *a, const void *b)
{
	TRACE(TRACE_FN, "" );
	int string1 = *(int*) a;
	int string2 = *(int*) b;

	int len = m_len(string1);

	if( len >  m_len(string2) )
		return 1;
	if( len <  m_len(string2) )
		return -1;
	
	/* empty array or array with just a zero */
	if( len == 0 || len == 1 ) return 0;

	TRACE(TRACE_ALL, "%s with %s", m_str(string1), m_str(string2) );	      
	return memcmp( m_buf(string1), m_buf(string2), len -1 );
}


void sort_stringlist(int list)
{
	TRACE(TRACE_FN, "" );
	 qsort( m_buf(list), m_len(list), m_width(list), compar );
}


/* performs glob() and returns the filenames as a list of handles to c-strings */
int glob_files( const char *s )
{
	TRACE(TRACE_FN, "" );
	int fl = m_create(50,sizeof(int));
	glob_t glob_result;
	int return_value;

	// Perform globbing
	return_value = glob(s, 0, NULL, &glob_result);
	if (return_value != 0) {
		if (return_value == GLOB_NOMATCH) {
			TRACE(1, "No matches found." );
		} else {
			WARN("Error in globbing." );
		}
		return fl;
	}

	// Iterate through the matched files
	for (size_t i = 0; i < glob_result.gl_pathc; i++) {
		copy_from_cstr(fl,glob_result.gl_pathv[i]);
	}

	// Free the memory allocated for the glob structure
	globfree(&glob_result);
	return fl;
}

/* use only basic functions to print the array to stdout */
void  dump_strings1(int strings)
{
	TRACE(TRACE_FN, "" );
	for( int i = 0; i < m_len(strings); i++ )
	{
		int h = *(int *) mls( strings, i );
		printf( "%s\n", (char *) mls(h,0) );
	}		
}

/* use the m_foreach macro to iterate the list */ 
void  dump_strings2(int strings)
{
	TRACE(TRACE_FN, "" );
	int i, *d;
	m_foreach(strings,i,d) {
		put_string( *d );
	}
}

/* free all strings then free the stringlist */ 
void free_string_list(int strings )
{
	TRACE(TRACE_FN, "" );
	int i, *d;
	m_foreach(strings,i,d) m_free(*d);		
	m_free(strings);
}


int main( int argc, char **argv )
{
	m_init();
	trace_level=1;
	TRACE(TRACE_FN, "" );
	
	int strings = glob_files("./*c");
	sort_stringlist(strings);
	dump_strings1(strings);

	trace_level=TRACE_FN;
	sort_stringlist(strings);
	dump_strings2(strings);

	free_string_list( strings );
	m_destruct();	
}
