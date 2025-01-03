#include <stdio.h>
#include "m_tool.h" // Assuming this is the header file for m_tool.c

// Function to demonstrate string concatenation
void demonstrate_string_concatenation() {
    // TODO: Implement string concatenation using functions from m_tool.c
    // Example: concatenate two strings and print the result
}

// Function to demonstrate string comparison
void demonstrate_string_comparison() {
    // TODO: Implement string comparison using functions from m_tool.c
    // Example: compare two strings and print whether they are equal or not
}

void demonstrate_string_sort()
{
	int src = s_cstr("Hello World, there is another World to visit");
	int pattern = s_cstr(" ");
	int dest = 0;
	int p,*d;
	
	dest = s_msplit( dest, src, pattern );

	printf("\n%s\n", __FUNCTION__ );
	printf("Unsorted list:\n");
	m_foreach( dest, p, d ) {
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d), *d & 0xffffff );
	}

	
	m_qsort( dest, cmp_mstr );
	printf("Sorted list:\n");

	m_foreach( dest, p, d ) {
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d), *d & 0xffffff );
	}
	
	m_free_list( dest );
}



	
void demonstrate_string_split() {
	int src = s_cstr("Hello World, there is another World to visit");
	int pattern = s_cstr("World");
	int dest = 0; // buffer alloced by s_msplit for a list of strings 

	dest = s_msplit( dest, src, pattern );

	printf("\n%s\n", __FUNCTION__ );
	printf("Original string: '%s' (id=%d)\n", m_str(src), src  & 0xffffff );
	printf("Pattern to split: '%s'\n", m_str(pattern));
	int p, *d;
	m_foreach( dest, p, d ) {
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d), *d & 0xffffff );
	}


	int dest2 = s_implode(0, dest, pattern );
	printf("Original string: '%s' (id=%d)\n", m_str(src), src  & 0xffffff );
	printf("Seperator: '%s'\n", m_str(pattern));
	printf("Merged string: '%s' (id=%d) \n", m_str(dest2), dest2  & 0xffffff  );
	int const_merged = s_mstr(dest2);
	printf("Merged string to constant: '%s' (id=%d)\n", m_str(const_merged), const_merged & 0xffffff  );
	
	
	m_free_list( dest );
	m_free( dest2 );
}

// Function to demonstrate string/array copy
void demonstrate_string_copy() {
	int src = s_cstr("Hello World");
	int dest = 0; // buffer alloced by s_slice
	int offs = 0; // target start at first char
	int start = 2; // start after 2nd byte
	int end = -1;  // -1 is the last character, -2 is the char before the last char
	
	dest = s_slice( dest, offs, src, start, end );

	printf("\n%s\n", __FUNCTION__ );
	printf("Original string: %s (id=%d)\n", m_str(src), src);
	printf("Resulting string: %s (id=%d)\n", m_str(dest), dest & 0xffffff );
	m_free(dest);
}

// Function to demonstrate string search
void demonstrate_string_search() {

	int src = s_cstr("Hello World, there is another World");
	int pattern = s_cstr("World");
	int offs = 0;

	printf("\n%s\n", __FUNCTION__ );
	printf("Original string: %s (id=%d)\n", m_str(src), src & 0xffffff );
	printf("Pattern to search: %s\n", m_str(pattern));
	while(   (offs = s_strstr( src, offs, pattern) ) != -1 ) {
		printf( "found at %d\n", offs );
		offs += s_strlen(pattern);
	}
}


void demonstrate_string_replacement() {

	int src = s_cstr("Hello World");
	int pattern = s_cstr("World");
	int replace = s_cstr("Universe");
	int dest = 0;  // buffer will be allocated by s_replace
	int count = 1; // Number of replacements to perform

	dest = s_replace(dest, src, pattern, replace, count);

	printf("%s\n", __FUNCTION__ );
	printf("Original string: %s (id=%d)\n", m_str(src), src);
	printf("Pattern to replace: %s\n", m_str(pattern));
	printf("Replacement string: %s\n", m_str( replace));
	printf("Resulting string: %s\n", m_str(dest));
	m_free(dest);
}


int main() {
  m_init();
  conststr_init();
  trace_level=0;
    
  printf("Advanced String Manipulation Demonstration\n");
  demonstrate_string_replacement();
  demonstrate_string_concatenation();
  demonstrate_string_comparison();
  demonstrate_string_split();
  demonstrate_string_copy();
  demonstrate_string_search();
  demonstrate_string_sort();
  conststr_free();
  m_destruct();
  return 0;
}
// Function to demonstrate string replacement
