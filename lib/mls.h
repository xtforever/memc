#ifndef MLS_H
#define MLS_H

#ifdef  __cplusplus
extern "C" {
#endif

/* $Id: mls.h,v 1.1.1.1 2010-02-12 08:04:52 jens Exp $ */
// 2007/11/23 nomux inserted debug.h and mlsdb.h removed SICHER
// 2011/10/03 nomux inserted lst_resize, m_resize
// 2012/07/17 nomux inserted macro CHAR
// 2012/07/18 nomux included mlsext, waste space? what space?
// 2014/02/04 nomux inserted m_remove
// 2014/05/06 nomux inserted m_buf
// 2014/12/09 nomux mstrcmp
// 2015/03/27 nomux s_lastchar
// 2018/06/16 nomux define CHARP
    
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#ifndef is_empty
#define is_empty(s) (!((s) && *(s)))
#endif

  
#ifndef ALEN
#define ALEN(x) (sizeof(x)/sizeof(*x))
#endif

#ifndef Max
#define Max(x,y) ((x)>(y) ? (x) : (y))
#endif

#ifndef Min
#define Min(x,y) ((x)>(y) ? (y) : (x))
#endif

#ifndef BIT
#define BIT(x) ( 1 <<(x) )
#endif

#define increase_by_percent(a,p)  calc_percent(a,p+100)
#define calc_percent(a,p)  (((p) > 0 ) ? (a) * (p) / 100 : 0)

#define ASERR(n,f,a...) do { if(!(n)) ERR( "ASSERT:"#n"\n"#f, ##a ); } while(0)
#define ASSERT(n) do { if( !(n) ) ERR( "ASSERT "#n ); } while(0)
#define ERR(n, a...) deb_err( __LINE__, __FILE__, __FUNCTION__,n , ## a  )
#define WARN(n, a...) deb_warn( __LINE__, __FILE__, __FUNCTION__, n , ## a )
#define TRACE(l, n, a... ) do { if( (l) >= trace_level && trace_level != 0 ) deb_trace( l, __LINE__, __FILE__, __FUNCTION__, n , ## a ); } while(0)
#define ERREX ERR("Schwerer Unbekannter Programmfehler")
#define TR(a) TRACE(a,"");

void deb_err( int line, const char* file, const char *function, const char *format, ... );
void deb_warn( int line, const char* file, const char *function, const char *format, ... );
void deb_trace( int l, int line, const char* file, const char *function, const char *format, ... );

#define CHARP(m) ((char*)m_buf(m))

extern int trace_level;

// trace_level:
//
// EX:
// TRACE( 1, "schleifendurchlauf: %d", i );
// TRACE( 2, "function entry" );
// TRACE( 3, "load_file success, %d bytes", nread );
//
// trace_level = 1 ; // show all TRACE comments
// trace_level = 2; // show TRACE comments with first argument equal or greater than 2



// ********************************************
//
// simple array handling
// foundation functions for mls
//
// ********************************************

typedef struct ls_st {
	int w, l, max;
	char uaf_protection;
	char d[0];
} *lst_t __attribute__(( aligned(1) ));

void* lst( lst_t l, int i )   __attribute__ ((pure));
lst_t lst_create(int max, int w);
int lst_new(lst_t *LP, int n);
int lst_put( lst_t *LP, const void *d );
int lst_next( lst_t l, int *p, void *data );
int lst_read( lst_t l, int p, void **data, int n);
int lst_write( lst_t *lp, int p, const void *data, int n);
void *lst_peek( lst_t l, int i );
void lst_del( lst_t l, int p );
void* lst_ins( lst_t *lp, int p, int n );
// we does not need lst_app, lst_dub, lst_copy : no optimization possible
void lst_resize( lst_t *LP, int new_size);

// ********************************************
//
// array handling functions
// arrays are identified by an integer
// handle and are indexed by an integer
//
// ********************************************

void* mls( int m, int i );
int m_new( int m, int n );
void *m_add( int m );
int m_next( int m, int *p, void *d );
int m_init();
void m_destruct() ;
int m_create(int max, int w);
int m_free(int h);
int m_put( int m, const void* data );
int m_len( int m );
int m_setlen( int m, int len );
int m_bufsize( int m );
void *m_buf( int m );
void *m_peek( int m, int i );
int m_write(  int m, int p, const void *data, int n );
int m_read(  int h, int p, void **data, int n  );
void m_clear(int m );
void m_del( int m, int p );
void* m_pop(int m);
int m_ins(int m, int p, int n);
int m_width( int m);
void m_resize( int m, int new_size);
void m_remove( int m, int p, int n );
static inline char *m_str(int m) { return m && m_len(m) ? m_buf(m) : ""; };

// ********************************************
//
// array handling  debugging functions
//
// _m_destruct will print
// each still allocated array
//
// _m_free will
// warn if you try to double-free an array
//
// in the case an error is detected,
// these functions will:
// show the caller (function, file, line) and
// show the possible cause of the error
//
// ********************************************

int _m_init();
void _m_destruct();
int _m_create(int ln, const char *fn, const char *fun,
		int n, int w);
int _m_free(int ln, const char *fn, const char *fun,
	      int m );
void* _mls( int ln, const char *fn, const char *fun,
	    int h, int i );
int _m_put( int ln, const char *fn, const char *fun,
	    int h, const void *d );
int _m_next( int ln, const char *fn, const char *fun,
	    int h, int *i, void *d );
void _m_clear( int ln, const char *fn, const char *fun,
	      int h );
void* _m_buf(int ln, const char *fn, const char *fun,
	      int m );

// ********************************************
//
// some defines to make life more fun
//
// ********************************************

#define m_foreach(lst,index,ptr) \
	for(index=-1; m_next(lst,&index,&ptr); )
#define STR(x,i) (*(char**)mls((x),(i)))
#define INT(x,i) (*(int*)mls((x),(i)))
#define UINT(x,i) (*(unsigned int*)mls((x),(i)))
#define CHAR(x,i) (*(char*)mls((x),(i)))
#define UCHAR(x,i) (*(unsigned char*)mls((x),(i)))
#define m_cat(h, s) m_write(h,m_len(h),(s),strlen((s)))
#define MSTR(x) ((char*)mls(x,0))
// ********************************************
//
// mls based RINGBUF
//
// ********************************************

    void ring_free(int r );
    int ring_get( int r );
    int ring_put( int r, int data );
    int ring_full(int r);
    int ring_empty(int r);
    int ring_create( int size );

// ********************************************
//
// UTILITIES, kiss
//
// ********************************************

	typedef char utf8_char_t[6];
	void m_bzero( int m );
	void m_free_strings(int list, int CLEAR_ONLY );
	void m_bzero( int m );
	void m_skip(int m, int n);
	int m_split(int m, const char *s, int c, int remove_wspace);
	int s_split(int m, const char *s, int c, int remove_wspace);

	int m_regex( int m, const char *regex, const char *s );
	int m_dub(int m);
	int m_fscan2( int m, char delim, FILE *fp);
	int m_fscan( int m, char delim, FILE *fp);
	int m_cmp( int a, int b );
	int m_lookup( int m, int key );
	int m_lookup_obj( int m, void *obj, int size );
	int utf8_getchar(FILE *fp, utf8_char_t buf );
	void m_putc(int m, char c);
	void m_puti(int m, int  c);
	int m_lookup_str(int m, const char *key, int NOT_INSERT);
	int utf8char(char **s);
	int m_utf8char(int buf, int *p);
	
	void m_qsort( int list, int(*compar)(const void *, const void *));
	int m_bsearch( const void *key, int list, int (*c)(const void *, const void *));
	int m_lfind(const void *key, int list, int (*c)(const void *, const void *));
	int m_binsert( int buf, const void *data, int (*cmpf) (const void *a,const void *b ), int with_duplicates );
	
// ********************************************
//
// UTILITIES, vars
//
// ********************************************

#define VAR_NAME(x) STR(x,0)

  int     v_init( void );
  void    v_free( int vs );

#define v_create v_init


// using variable name
int     v_set( int vs, const char* name, const char* value, int pos );
void    v_vaset( int vs, ... );
void    v_clr( int vs, const char* name );
char*   v_get( int vs, const char* name, int pos );
int     v_len( int vs, const char* name );
void    v_remove( int vs,const  char* name );
int     v_lookup( int vs, const char* name );
int     v_find_key(int vs, const char *name);
// using variable index
void    v_kset( int key, const  char* value, int pos );
void    v_kclr( int key );
char*   v_kget( int key, int pos );
int     v_klen( int key );

// expand strings with embedded variables
typedef struct str_exp_st {
  int splitbuf;
  int max_row;
  int values; // [char*]
  int indices; // [int]
  int buf;
} str_exp_t;

void    se_init(  str_exp_t *se  );
void    se_free(  str_exp_t *se );
void    se_realloc_buffers( str_exp_t *se );
void    se_parse(str_exp_t *se, const char *frm);
char*   se_expand( str_exp_t *se, int vl, int row );
char*   se_string( int vl, const char *frm );

// utility, mysql_escape_string compatible
// create a escaped string from src into buf
int     escape_str( int buf, char *src );
// only escape. do not alloc buf, no zero termination
void    escape_buf( int buf, char *src );

  /* get length of string m without trailing zero */
int s_strlen(int m );
  /* append strings s1,s2,s3,... to m */
int s_app(int m, ...) __attribute__ ((__sentinel__(0)));
    /* append string |s| to |m| */
int s_app1(int m, char *s);
  /* write sprintf(...) string to array m at pos p */
int vas_printf(int m, int p, const char *format, va_list argptr );
int s_printf(int m, int p, char *format, ...);
int s_index( int buf,int p, int ch );
int mstrcmp(int m,int p, const char *s);
int mstr_to_long(int buf, int *p, long int *ret_val);
int s_lastchar(int m);
int s_copy( int m, int first_char, int last_char  );

  /* array m is an empty string if:
   1) no array given
   2) array length is zero
   3) first byte is zero
*/
static inline int s_empty(int m)
{
	return ! (m && m_len(m) && CHAR(m,0));
}



enum {
  VAR_APPEND = -1,
  VAR_RENAME = 0,
  VAR_SET    = 1
};

// ********************************************
//
// Overwrite mls_xxx protos to enable debuging
// if you dont need the debug version
// simply do not define MLS_DEBUG
//
// ********************************************

#ifdef  __cplusplus
}
#endif


#endif


#if ! defined(MLS_DEBUG_ACTIVE) && ! defined( MLS_DEBUG_DISABLE)


#ifdef MLS_DEBUG
#define MLS_DEBUG_ACTIVE

#define m_init() _m_init()
#define m_destruct() _m_destruct()

#define mls(m,i) \
	_mls(__LINE__, \
	__FILE__,__FUNCTION__,(m),(i))

#define m_create(n,w) \
	_m_create(__LINE__, \
	__FILE__,__FUNCTION__,(n),(w))

#define m_free(m) \
	_m_free(__LINE__, \
	__FILE__, __FUNCTION__,(m))

#define m_buf(m)         \
	_m_buf(__LINE__, \
	__FILE__, __FUNCTION__,(m))

#define m_put(m,d) \
	_m_put(__LINE__, \
	__FILE__,__FUNCTION__,(m),(d))

#define m_next(m,i,d) \
	_m_next(__LINE__, \
	__FILE__,__FUNCTION__,(m),(i),(d))

#define m_clear(m) \
	_m_clear(__LINE__, \
	__FILE__,__FUNCTION__,(m))

#endif // MLS_DEBUG
#endif // MLS_DEBUG_DISABLE
