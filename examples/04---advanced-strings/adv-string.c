#include "m_tool.h"
#include <printf.h>
#include <stdio.h>

#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"

void
demonstrate_string_comparison()
{
	printf("\n%s\n", __FUNCTION__);

	int res = 0;
	int hs1 = s_cstr("World");
	int hs2 = s_cstr("Hello");
	const char *s3 = "Kilo";

	res = mstrcmp(hs1, 0, s3);
	printf("compare mstr %M with cstring %s: result=%d\n", hs1, s3, res);

	res = m_cmp(hs2, hs1);
	printf("compare mstr %M with mstr %M: result=%d\n", hs2, hs1, res);

	// if using const-string you have the same handle for the same string
	int hs4 = cs_printf("%s", "World");
	printf("compare mstr %M with mstr %M: result=%s\n", hs1, hs4,
	       hs1 == hs4 ? "true" : "false");
}

void
demonstrate_string_concatenation()
{
	printf("\n%s\n", __FUNCTION__);
	int s1 = 0;
	int s2 = 0;
	int s3 = 0;
	s1 = s_app(0, "Hello ", "World, ", NULL);
	s_app(s1, "there ", "is ", "another ", "World ", "to ", "visit", NULL);
	printf("String s1: '%M'\n", s1);

	//          using s_printf and custom specifier to concat a string
	s2 = s_printf(0, 0, "You said: <%M>", s1);
	printf("String s2: '%M'\n", s2);

	//          using m_slice to copy first 6 chars to new string
	s3 = m_slice(0, 0, s1, 0, 5);

	//          using m_slice to append a part of string
	m_slice(s3, -1       , s2, 16, 20);
	m_putc(s3, 0);
	printf("String s3: '%M'\n", s3);
	
	//          using m_slice to append to a string
	s_printf(s1, 0, "m_slice can be used " );
	s_printf(s2, 0, "to append strings\n\n" );
	m_slice(s1, -2, s2, 0, -1); /* Instead of using the end of s1
				     * (-1), we target the position of
				     * its null terminator (-2) */
	printf("String s1: '%M'", s1);	

	//          using s_printf to concat
	s_printf(s1, 0, "s_printf can be used " );
	s_printf(s1, -1, "to append strings\n\n" );
	printf("String s1: '%M'", s1);	

	m_free(s1);
	m_free(s2);
	m_free(s3);
}

void
demonstrate_string_sort()
{
	int src = s_cstr("Hello World, there is another World to visit");
	int pattern = s_cstr(" ");
	int dest = 0;
	int p, *d;

	dest = s_msplit(dest, src, pattern);

	printf("\n%s\n", __FUNCTION__);
	printf("Unsorted list:\n");
	m_foreach(dest, p, d)
	{
		s_lower(*d);
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d),
		       *d & 0xffffff);
	}

	m_qsort(dest, cmp_mstr);
	printf("Sorted list:\n");

	m_foreach(dest, p, d)
	{
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d),
		       *d & 0xffffff);
	}

	m_free_list(dest);
}

void
demonstrate_string_split()
{
	int src = s_cstr("Hello World, there is another World to visit");
	int pattern = s_cstr("World");
	int dest = 0; // buffer alloced by s_msplit for a list of strings

	dest = s_msplit(dest, src, pattern);

	printf("\n%s\n", __FUNCTION__);
	printf("Original string: '%s' (id=%d)\n", m_str(src), src & 0xffffff);
	printf("Pattern to split: '%s'\n", m_str(pattern));
	int p, *d;
	m_foreach(dest, p, d)
	{
		printf("Resulting string: '%s' (id=%d)\n", m_str(*d),
		       *d & 0xffffff);
	}

	int dest2 = s_implode(0, dest, pattern);
	printf("Original string: '%s' (id=%d)\n", m_str(src), src & 0xffffff);
	printf("Seperator: '%s'\n", m_str(pattern));
	printf("Merged string: '%s' (id=%d) \n", m_str(dest2),
	       dest2 & 0xffffff);
	int const_merged
	    = s_mstr(dest2); /* convert string to constant string */
	printf("Merged string to constant: '%s' (id=%d)\n", m_str(const_merged),
	       const_merged & 0xffffff);

	m_free_list(dest);
	m_free(dest2);
}

// Function to demonstrate string/array copy
void
demonstrate_string_copy()
{
	int src = s_cstr("Hello World");
	int dest = 0;  // buffer alloced by s_slice
	int offs = 0;  // target start at first char
	int start = 0; // start at first byte
	int end = -1;  // -1 is the last character, -2 is the char before the
	               // last char

	dest = s_slice(dest, offs, src, start, end);

	printf("\n%s\n", __FUNCTION__);
	printf("Original string: %s (id=%d)\n", m_str(src), src);
	printf("Resulting string: %s (id=%d)\n", m_str(dest), dest & 0xffffff);
	m_free(dest);
}

// Function to demonstrate string search
void
demonstrate_string_search()
{

	int src = s_cstr("Hello World, there is another World");
	int pattern = s_cstr("World");
	int offs = 0;

	printf("\n%s\n", __FUNCTION__);
	printf("Original string: %s (id=%d)\n", m_str(src), src & 0xffffff);
	printf("Pattern to search: %s\n", m_str(pattern));
	while ((offs = s_strstr(src, offs, pattern)) != -1) {
		printf("found at %d\n", offs);
		offs += s_strlen(pattern);
	}
}

void
demonstrate_string_replacement()
{

	int src = s_cstr("Hello World");
	int pattern = s_cstr("World");
	int replace = s_cstr("Universe");
	int dest = 0;  // buffer will be allocated by s_replace
	int count = 1; // Number of replacements to perform

	dest = s_replace(dest, src, pattern, replace, count);

	printf("%s\n", __PRETTY_FUNCTION__);
	printf("Original string: %s (id=%d)\n", m_str(src), src);
	printf("Pattern to replace: %s\n", m_str(pattern));
	printf("Replacement string: %s\n", m_str(replace));
	printf("Resulting string: %s\n", m_str(dest));
	m_free(dest);
}

void
demonstrate_printf()
{
	printf("\n%s\n", __FUNCTION__);
	m_register_printf();

	int src = s_cstr("Hello World");
	printf("String: '%-14.5M'\n", src);
	printf("String: '%-14.5s'\n", m_str(src));

	printf("String: '%M'\n", src);
	printf("String: '%s'\n", m_str(src));

	printf("String: '%14M'\n", src);
	printf("String: '%14s'\n", m_str(src));

	printf("String: '%.5M'\n", src);
	printf("String: '%.5s'\n", m_str(src));
}

int
main()
{
	m_init();
	conststr_init();
	trace_level = 1;

	printf("Advanced String Manipulation Demonstration\n");
	demonstrate_printf();
	demonstrate_string_concatenation();
	demonstrate_string_comparison();
	demonstrate_string_replacement();

	demonstrate_string_split();
	demonstrate_string_copy();
	demonstrate_string_search();
	demonstrate_string_sort();

	conststr_free();
	m_destruct();
	return 0;
}
