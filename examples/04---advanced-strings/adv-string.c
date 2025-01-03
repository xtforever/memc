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

// Function to demonstrate string length calculation
void demonstrate_string_length() {
    // TODO: Implement string length calculation using functions from m_tool.c
    // Example: calculate the length of a string and print the result
}

// Function to demonstrate string/array copy
void demonstrate_string_copy() {
	int src = s_cstr("Hello World");
	int dest = 0;

	dest = m_slice( dest, 0, src, 0, -1 );
	printf("Original string id: %d\n", src );
	printf("Resulting string: %s (id=%d)\n", m_str(dest), dest );
	m_free(dest);
}

// Function to demonstrate string search
void demonstrate_string_search() {
    // TODO: Implement string search using functions from m_tool.c
    // Example: search for a substring within a string and print the result
}


void demonstrate_string_replacement() {

  int src = s_cstr("Hello World");
  int pattern = s_cstr("World");
  int replace = s_cstr("Universe");
  int dest = 0;  // buffer will be allocated by s_replace
  int count = 1; // Number of replacements to perform

  dest = s_replace(dest, src, pattern, replace, count);

  printf("Original string: %s\n", m_str(src));
  printf("Pattern to replace: %s\n", m_str(pattern));
  printf("Replacement string: %s\n", m_str( replace));
  printf("Resulting string: %s\n", m_str(dest));
  
}


int main() {
  m_init();
  conststr_init();
  trace_level=1;
    
  printf("Advanced String Manipulation Demonstration\n");
  demonstrate_string_replacement();
  demonstrate_string_concatenation();
  demonstrate_string_comparison();
  demonstrate_string_length();
  demonstrate_string_copy();
  demonstrate_string_search();

  conststr_free();
  m_destruct();
  return 0;
}
// Function to demonstrate string replacement
