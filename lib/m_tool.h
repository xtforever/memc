#ifndef M_TOOL_H
#define M_TOOL_H
/* experimental tools */


#include "mls.h"
#include <stdarg.h>
#include <stdbool.h>

/* how man strings before forced exit(122) to prevent OOM Killer */ 
#define CONSTSTR_MAX 9000

void m_register_printf(void);

/* m string (ms) : m-array of (char*) */
int m_str_va_app(int ms, va_list ap);
int m_str_app(int ms, ...);
int m_str_split(int ms, char *s, char* delim, int trimws );


int s_strcmp_c( int mstr, const char *s );
int m_strncpy(int dst, int src, int max);
int m_mcopy(int dest, int destp, int src, int srcp, int src_count  );
int m_binsert( int buf, const void *data, int (*cmpf) (const void *a,const void *b ), int with_duplicates );
int compare_int(const void *a,const void *b);
int cmp_mstr_fast(const void *a, const void *b);
int cmp_mstr_cstr_fast(const void *a, const void *b);
int m_str_from_file( char *filename );

void m_free_ptr(void *d);
void m_free_user(int m, void (*free_h)(void*), int only_clear );
void m_clear_user( int m, void (*free_h)(void*) );
void m_free_list(int m);
void m_clear_list(int m);
void m_clear_stringlist(int m);
void m_free_stringlist(int m);

void m_concat(int a, int b);
int m_split_list( const char *s, const char *delm );

int leftstr(int buf, int p, const char *s, int ch);
int cmp_mstr(const void *a, const void *b);
int cmp_int( const void *a0, const void *b0 );
int lookup_int(int m, int key);
int m_slice(int dest, int offs, int m, int a, int b );
int s_slice(int dest, int offs, int m, int a, int b );
int s_replace(int dest, int src, int pattern, int replace, int count );
void s_puts(int m);
int s_strstr(int m, int offs, int pattern );
int s_strncmp(int m,int offs,int pattern, int n);
void s_write(int m,int n);
int s_isempty(int m);
int s_strdup_c(const char *s);
int s_strcpy_c(int out, const char *s);
int s_trim(int m);
int s_lower(int m);
int s_upper(int m);
int s_msplit(int dest, int src, int pattern );
int s_implode(int dest, int srcs, int seperator );

void m_map( int m, int (*fn) ( int m, int p, void *ctx ), void *ctx  );
int  m_memset(int ln, char c, int w); 
int s_strncmp2(int s0, int p0, int s1, int p1, int len);
int s_strncmpr(int str, int suffix);

int s_readln(int buf, FILE *fp);
int s_regex(int res, char *regex, int buf);
bool glob_match(char const *pat, char const *str, const char **a, const char **b) __attribute__((const));

/* imported from conststr */
void conststr_stats(void);
void conststr_init(void);
void conststr_free(void);
int conststr_lookup_c(const char *s);
int conststr_lookup(int s);

/*
  s_cstr - returns a handle to a constant string equal to the given c-style string 
  s_mstr - returns a handle to a constant string equal to the given array 
  cs_printf - creates a string and returns a handle to a constant string equal to the created one
*/
int cs_printf( const char *format, ... );
inline static int s_cstr(const char *s) { return conststr_lookup_c(s); }
inline static int s_mstr(int m) { return conststr_lookup(m); }


#endif
