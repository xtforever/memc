# ifdef S_SPLINT_S
#define __BEGIN_DECLS
# endif

#define MLS_DEBUG_DISABLE
#include "mls.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <search.h>
#include <limits.h>

static const char *Version="Version:$Id: mls.c,v 1.1.1.1 2010-02-12 08:04:52 jens Exp $";

// HISTORY:
//
// 2007/11/23 nomux BUG: lst_new, m_new: LP->max not updated when array is resized
//
// 2007/12/18 nomux OPT: m_free, m_create: dont scan for free entries, just put them into a new list.
//
// 2007-12-25 nomux: m_free(): ignore h==0 instead of abort with error
//
// 2011-02-04 nomux: m_ins: added n==0 -> error, keine erlaubnis 0 elemente einzufÃ¼gen
//
// 2012-03-09 nomux: deb_err: removed "debi.me=0". es sollte verhindert werden das nach einem internen fehler noch
// ueberflÃ¼ssige melden erscheinen. dies funktionierte ganz und gar nicht. stattdessen wurde die fehler-rÃ¼ckverfolgung abgeschaltet.
//
// 2012-03-12 nomux: m_setlen verwendet jetzt lst_resize um neuen speicher zu allokieren

// 2014-02-16 lst_remove remove multiple items bug, m_init, _m_init do not terminate prog
// 2014-06-30 lst_resize, lst_create - fill allocated memory with  0
// 2014-07-09 m_utf8getchar - benutzt jetzt neue UTF8CHAR
// 2014-07-09 void m_qsort( int list, int(*compar)(const void *, const void *))
// 2016-11-27 mstr_to_long
// 2018-12-28 m_write, lst_write
//            lst_write bug: if m_len(m) and count=0 -> error
// 2019-03-13 BUG: buffer overflow in deb_xxx better use vsnprintf :-)
// 2021-05-11 m_next do nothing if list==0 
// 2021-11-22 BUG: lst_resize - fill new memory with zero
// 2023-12-18 FEATURE: m_next : allow data==NULL
// 2024-03-20 m_free_list( (void) user_free( void*, void* ), void *user_data )
// 2024-06-09 BUG: s_index missing p++ 
// -----------------------------------------------------------------------------------------------------

struct lst_owner_st {
  int allocated;
  const char *fn, *fun;
  int ln;
};
typedef struct lst_owner_st lst_owner;

static int DEB=0; // debug list

struct debug_info_st {
  char msg[500];
  const char *me,*fn,*fun;
  int ln, args, handle, index;
  const void * data;
};

static struct debug_info_st debi;


//
// Error Reporting
// Run-time TRACE
//

int trace_level = 0;
static char buf[4096];

void deb_err( int line, const char* file, const char *function, const char *format, ... )
{

    va_list argptr;
    int err = errno;
    va_start(argptr, format);
    vsnprintf(buf, sizeof(buf), format, argptr);
    va_end(argptr);

    fprintf(stderr, "ERROR: %s Line: %d Function: %s\n%s\n", file, line, function, buf );
    // todo: error handler exception for errors in mls.c
    // debi.me=0; // do not inspect mls functions for debug info
    if( err ) perror("");
    exit(1);
}

void deb_warn( int line, const char* file, const char *function, const char *format, ... )
{

   va_list argptr;


   va_start(argptr, format);
   vsnprintf(buf,sizeof(buf), format, argptr);
   va_end(argptr);

   fprintf(stderr, "WARNING: %s Line: %d Function: %s\n%s\n", file, line, function, buf );
}

void deb_trace( int l, int line, const char* file, const char *function, const char *format, ... )
{

   va_list argptr;
   (void) line;
   (void) file;

   va_start(argptr, format);
   vsnprintf(buf,sizeof(buf), format, argptr);
   va_end(argptr);

   fprintf(stderr, "[%d]%s: %s\n", l, function, buf );
}

//
// Error Checking:
//   lst
//     Out of Memory --> exit(1)
//     Wrong Args    --> exit(1)
//
//
//  mlsdb
//     saves caller
//     gathers informations on alloc/free
//
//
// ********************************************
//
// lst_XXX and
// m_XXX implementation
//
// ********************************************

static lst_t ML = 0; // stack allocated vars
static lst_t FR = 0; // stack freed vars



int print_stacksize()
{
  printf("STACKSIZE: %d\n", ML->l );
  return 0;
}

/** this functions calls realloc and fills the newly allocated memory with zeros 
 * in case realloc returns zero this exits with an error message 
 */
void* reallocz(void* pBuffer, size_t oldSize, size_t newSize)
{
  void* pNew = realloc(pBuffer, newSize);
  if( !pNew ) ERR("could not realloc buffer");
  if ( newSize > oldSize && pNew ) {
    size_t diff = newSize - oldSize;
    void* pStart = ((char*)pNew) + oldSize;
    memset(pStart, 0, diff);
  }
  return pNew;
}

// returns: ptr to element i
// R: NULL - index out of bounds
void* lst( lst_t l, int i )
{
  if ( i >= l->l || i < 0 ) ERR("Index(%d) out of Bounds", i);
  return &l->d[l->w*i];
}

//!X!
// R: NULL - out of memory
lst_t lst_create(int max, int w)
{
    lst_t l=(lst_t)calloc(1, (max*w) + sizeof(struct ls_st) );
    if( !l) ERR("Out of Memory");
    l->max = max; l->l=0; l->w = w;
    return l;
}

// alloc space for (n) items
// if n == 1: if neccessary optimize resize
// if n > 1: alloc at most n items
// returns: -1 - error, >=0 - index of new item
int lst_new(lst_t *LP, int n)
{
  int max =  (**LP).max;
  int len =  (**LP).l;
  int space = max - len - n;

  // not enough space left
  if( space < 0 ) {
    if( space == -1 ) // resize by 1.5
      max = (max >> 1) +1 + max;
    else
      max = len + n;
    int new_size = max * (**LP).w + sizeof(struct ls_st);
    int old_size = (**LP).max * (**LP).w + sizeof(struct ls_st);
    *LP = (lst_t) reallocz(*LP, old_size, new_size );
    (**LP).max = max;
  }
  (**LP).l += n;
  return len;
}

void lst_resize(lst_t *LP, int new_len)
{
  int len =  (**LP).l;
  if( new_len < 0 ) ERR("need new_size>=0. but new_size=%d", new_len );
  int new_size = new_len * (**LP).w + sizeof(struct ls_st);
  int old_size = (**LP).max * (**LP).w + sizeof(struct ls_st); 
  if( new_size == old_size ) return;
  *LP = (lst_t) reallocz(*LP, old_size, new_size );
  (**LP).max = new_len;
  if( new_len < len ) (**LP).l = new_len;
}

// append item
// returns: -1: error, index of new item
int lst_put( lst_t *LP, const void *d )
{
    int n;
    if( !d ) ERR("NULL-Ptr");
    n = lst_new(LP,1);
    if( n >=0 ) memcpy( lst(*LP,n), d, (*LP)->w );
    return n;
}

// get ptr to element after *p and increment p by one
// if *p is out of range *p is set to array-length
// params: p - pointer to index-var. *p should be init.
// with -1, data is ptr to ptr to array data.
// returns: 1 - ok and *data is a ptr to
// element, 0 - no more elements
int lst_next( lst_t l, int *p, void *data )
{
    *p += 1;
    if( *p > l->l || *p < 0 )
      { *p = (int) l->l; return 0; }
    if( *p ==(int) l->l ) return 0;
    if(data) *(void**)data = lst( l, *p );
    return 1;
}

// exits if realloc fails. returns zero if arg (p) is out of bounds
void* lst_ins( lst_t *lp, int p, int n )
{
  int cnt; void *src, *dst; lst_t l=*lp;
  if( (uint)p > (uint)l->l ) return NULL;
  if( l->l + n > l->max )
      {    // Optimize resize if we resize by one
	int old_size = l->max*l->w+sizeof(struct ls_st);	
	int new_max = l->l + n;
	if( new_max - l->max == 1 ) new_max <<= 1;
	l->max = new_max;
	int new_size  = l->max*l->w+sizeof(struct ls_st);      
	l=(lst_t)reallocz(l,old_size,new_size);
	*lp=l;
      }
  cnt = (l->l -p ) * l->w;
  l->l += n;
  src = lst(l,p);
  if( cnt > 0 ) // Do not app
    {
      dst = lst(l,p+n);
      memmove( dst,src, cnt );
    }
  bzero(src,n * l->w);
  return src;
}


// remove item inside array
void lst_del( lst_t l, int p )
{
  void *dest, *src;
  size_t n;

  if( l->l > p+1 )
    {
      dest = lst( l, p );
      src = lst( l, p+1 );
      n = l->l - p -1;
      if( n ) {
	n *= l->w;
	memmove( dest, src, n );
      }
    }
  l->l--;
}

// remove n items inside array starting at p
void lst_remove( lst_t *lp, int p, int n )
{
  void *dest, *src;
  lst_t l = *lp;
  size_t len;

  if( n<=0 || p < 0 ) return;
  if( p >= l->l ) return;
  if( p+n >= l->l ) {
    l->l = p;
    return;
  }

  dest = lst( l, p );
  src = lst( l, p+n );
  len = l->l -p -n;
  len *= l->w;
  memmove( dest, src, len );
  l->l-=n;
}

//!X!
// returns address of element i
void *lst_peek( lst_t l, int i )
{
  if( i < 0 || i > l->max ) ERR("Out of bounds");
  return &l->d[l->w*i];
}

// returns zero on succ.
// n==0 is aceptable, no error
int lst_write( lst_t *lp, int p, const void *data, int n)
{
  void *mem;
  lst_t l = *lp;
  if( p<0 || n<0 ) return -1;
  if( n == 0 ) return 0;
  if( p+n > l->max )
    {
      l->max = p+n;
      l=(lst_t)realloc(l,l->max*l->w+sizeof(struct ls_st));
      if(!l) ERR("could not realloc");
      *lp=l; // write-back new ptr
    }
  if( p+n > l->l) l->l=p+n;
  mem = lst(l,p);
  memcpy( mem, data, n*l->w );
  return 0;
}

// copy n items from array l at (p) to *data
// if *data == 0 alloc memory
int lst_read( lst_t l, int p, void **data, int n)
{
  if( p <0 || n<0 || data==NULL ) ERR("Wrong arguments");
  if( *data == 0 ) *data = malloc( l->w * n );
  if(! *data ) ERR("Out of Memory");
  memcpy( *data, lst(l,p), n * l->w );
  return 0;
}

// ********************************************
//
// array handling functions
//
//
// ********************************************

// hole zeiger auf das array m
// ist das array nicht allociert oder
// ist ML nicht init. oder m ausserhalb der
// mÃ¶glichen grenzen wird NULL Ã¼bergeben
// ansonsten ein zeiger auf auf den array-header
static inline lst_t* _get_list(int m)
{
  if(ML==0 || m < 1 ) ERR("Not initialized");
  lst_t *l = (lst_t*) lst( ML, m );
  if( *l == NULL ) ERR("List %d not allocated",m);
  return l;
}

void* mls( int m, int i )
{
  lst_t *lp = _get_list(m);
  return lst( *lp, i );
}

/* add n elements, return index of first element */
int m_new( int m, int n )
{
  lst_t *lp = _get_list(m);
  return lst_new( lp, n );
}

/* add single element, return its address */
void *m_add(int m)
{
    return mls( m, m_new(m,1));
}


void m_resize(int m, int new_size )
{
  lst_t *lp = _get_list(m);
  return lst_resize( lp, new_size );
}

// remove n items inside array starting at p
void m_remove( int m, int p, int n )
{
  lst_t *lp = _get_list(m);
  return lst_remove( lp, p, n );
}



/**
    @brief hole den zeiger auf das folgende element
    @param p - zeiger auf das zuletzt geholte element, -1 falls das erste gewünscht wird
    @param d - zeiger auf zeiger auf die gewünschte datenstruktur
    @param m - die liste
    @return 1 if element exists, 0 otherwise
*/
int m_next( int m, int *p, void *d )
{
    if(!m || !m_len(m) ) return 0;
    lst_t *lp = _get_list(m);
    if( !d ) ERR("Data address d is ZERO");
    return lst_next(*lp,p,d);
}

// returns 0 - ok, 1 - liste schon initialisiert
int m_init()
{
  static lst_t zero = 0;
  if( ML ) return 1; // schon initialisiert
  ML = lst_create( 100, sizeof( lst_t ));
  lst_put( &ML, &zero );
  FR = lst_create( 100, sizeof( int ));
  return 0;
}

// returns ZERO
void m_destruct()
{
    int p;
    lst_t *d;
    if( !ML ) ERR("Not Init.");
    for( p=-1; lst_next( ML, &p, &d); )
      if(*d) { free(*d); TRACE(1,"m_free %d\n", p ); }
    free(ML); ML=0;
    free(FR); FR=0;
}




int  m_create(int max, int w)
{
  int i;
  lst_t lp;
  if( ! ML || max <= 0 || w <= 0 ) ERR("Wrong args");
  lp = lst_create(max,w);

#if 0
  lst_t *t;
  for(i=ML->l-1;i>0; i--) {
    t = (lst_t*) lst(ML,i); // pointer to memory for lst_t
    ASSERT(t); // should always be a valid ptr
    if( *t == NULL ) {
      *t =  lp;
      return i;
    }
  }

#else

#if 0
  int stopper;
  /*
    suche vom letzten element der liste ML rückwärts zum zweiten
    nach einem NULL-Ptr.
    sonst anhängen

    suche nur die obersten 100 der liste ab
  */
  i = ML->l -1;
  t = ((lst_t*) ML->d) + i;

  if( i > 1000 )
    stopper = i-100;
  else
    stopper = ML->l * 0.9;

  while( i > stopper ) {
    if( *t == NULL ) {
      *t =  lp;
      return i;
    }
    t--;
    i--;
  }

#else

  // falls FR->l > 0 nehme freie plätze aus FR
  if( FR->l > 0 ) {
    i = *(int *) lst( FR, FR->l-1 );
    FR->l--;
    *(lst_t*) lst(ML, i) = lp;
    return i;
  }

#endif
#endif

  i= lst_put( &ML, &lp );
  return i;
}

//!X!
// free memory for list h
// Returns Zero
//
// OPT: speichert die nummer des freigegebenen arrays
// JH 2007-12-25 ignore h==0 instead of abort with error
int m_free(int h)
{
  if( !ML || h < 0 ) ERR("Wrongs Args ML=%p h=%d", ML,h);
  if( !h ) return 0;
  lst_t *l = (lst_t*) lst( ML, h );
  if( *l == NULL ) ERR("List %d starts at ZERO", h);
  free(*l);
  *l=0;

  lst_put( &FR, &h );

  return 0;
}

// append *data to array m
// returns: -1 error, >=0 index of new item
int m_put( int m, const void* data )
{
  lst_t *lp =_get_list(m);
  return lst_put(lp,data);
}

int m_len( int m )
{
  lst_t *lp =_get_list(m);
  return (**lp).l;
}

void *m_buf( int m )
{
    return m_peek(m,0);
}

int m_bufsize( int m )
{
  lst_t *lp =_get_list(m);
  return (**lp).max;
}

/**
 * @brief Insert 'n' elements at position 'p' in the array pointed to by 'm'.
 * 
 * Inserts 'n' elements at position 'p' in the array pointed to by 'm'.
 * If 'p' is negative or 'n' is non-positive, an error is raised.
 * 
 * @param m Pointer to the array where elements are to be inserted.
 * @param p The position at which to insert the elements.
 * @param n The number of elements to insert.
 * @return The start position 'p' after insertion.
 * 
 * @note This function assumes 'm' points to a valid array.
 * @note The position 'p' should be within the bounds of the array.
 */
int m_ins(int m, int p, int n)
{
  lst_t *lp;
  if( p < 0 || n <= 0 ) ERR("Wrong Args Start:%d Count:%d",p,n);
  lp=_get_list(m);
  lst_ins( lp, p, n );
  return p;
}

// !X!
// decrements the number of used elements
// returns a ptr to the last item, or NULL.
void* m_pop(int m)
{
  lst_t *lp;
  lp=_get_list(m);
  if( (**lp).l < 1 ) return NULL;
  (**lp).l--;
  return (*lp)->d + ( (*lp)->w * (*lp)->l );
}

// remove element
void m_del( int m, int p )
{
  lst_t *lp;
  if( p < 0 ) return;
  lp=_get_list(m);
  lst_del( *lp, p );
}

// clears array, sets used ptr to zero
void m_clear( int m )
{
     lst_t *lp;
     lp=_get_list(m);
     (**lp).l = 0;
}

/**
 * @brief Set the length of the array pointed to by 'm' to the specified length.
 * 
 * Sets the length of the array pointed to by 'm' to the specified length 'len'.
 * If 'len' is greater than the current maximum length of the array, the array
 * is resized to accommodate the new length. If 'len' is negative, an error is raised.
 * 
 * @param m Pointer to the array whose length is to be set.
 * @param len The new length to set for the array.
 * @return 0 on success, -1 on error.
 * 
 * @note This function only grows the array and does not shrink it.
 */
int m_setlen( int m, int len )
{
  lst_t *lp;
  if( len < 0 ) ERR("Wrong Arg len=%d", len);
  lp=_get_list(m);
  if( len > (**lp).max ) lst_resize( lp, len );
  (**lp).l = len;
  return 0;
}

void *m_peek( int m, int i )
{
  lst_t *lp;
  lp=_get_list(m);
  return lst_peek( *lp, i );
}

// copy |n| elements from |data| to array |m| at position |p|
// returns the array |m|
int m_write(  int m, int p,
		  const void *data, int n  )
{
  lst_t *lp;
  if( n <=0 ) return m;
  lp=_get_list(m);
  lst_write( lp, p, data, n);
  return m;
}

// copy n items from array (m) at (p) to data
int m_read(  int m, int p, void **data, int n  )
{
  lst_t *lp;
  lp=_get_list(m);
  return lst_read( *lp, p, data, n);
}

int m_width(int m)
{
  lst_t *lp;
  lp=_get_list(m);
  return (**lp).w;
}





// ********************************************
//
//  Debug-Function Implementation
//
// ********************************************
#define CASSERT(a,l,f,n) \
	ASERR( a, "Caller %s() in %s:%d", (n),(f),(l))




// !X!
// create caller message in the form:
// Funtion: _m_put was called by loop_test:2142 in 'rounders.c'
// args:
// bit 0: check handle
// bit 1: check index
// bit 2: check data
//
static void _mlsdb_caller(const char *me,
			  int ln, const char *fn, const char *fun,
			  int args, int handle, int index,
			  const void* data )
{
  debi.me = me;
  debi.ln = ln; debi.fn = fn; debi.fun = fun;
  debi.args=args; debi.handle = handle; debi.index=index;
  debi.data=data;
}

static void perr( char *format, ... )
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    fputc( '\n', stderr );
    va_end(argptr);
}

//!X!
// check for handle error
// R: 0 if handle seems correct
static int _mlsdb_check_handle()
{
  lst_t *lp;
  lst_owner *o;
  int h = debi.handle;
  perr("Checking Handle %d", h );

  if( h < 1 || h > m_len(DEB) ) {
    perr( "Handle out of range (0 < %d < %d)",
	  h, m_len(DEB)+1 );
    return -1;
  }

  o = (lst_owner*) mls( DEB, h-1 );

  if( !o || o->allocated != 42 )
    {
      perr("Array was not allocated" );
      return -1;
    }

  if( o->ln < 0 )
    {
      perr("Array was previously removed by %s() in %s:%d",
	   o->fun, o->fn, o->ln );
      return -1;
    }

  perr("Array was created by %s in %s:%d",
       o->fun, o->fn, o->ln );

  lp = _get_list(h);
  perr("Base Address Structure: %p\n"
       "Base Address Data (d):%p\nElem.Width (w):%d\n"
       "Buffer size(max):%d\nUsed Size(l):%d",
       *lp, (*lp)->d, (*lp)->w,(*lp)->max,(*lp)->l );
  return 0;
}
static int _mlsdb_check_index()
{
  int i = debi.index, h = debi.handle;
  if( i < 0 ) {
    perr( "Array Index %d should be >=0\n", i );
    return -1;
  }

  if( i >= m_len(h) ) {
    perr( "Array Index out of bounds i=%d should be lower than m_len(%d)=%d.\n",
	  i, h, m_len(h) );
    return -1;
  }

  return 0;
}

void exit_error()
{
  if(! debi.me ) return;

  perr( "ERROR in funtion: '%s'. Called by '%s:%d' in '%s'",
	  debi.me, debi.fun, debi.ln, debi.fn );

  if( !ML ) {
    perr("m_init not called");
    return;
  }

  if( debi.args & 1 )
    if( _mlsdb_check_handle() ) return;

  if( debi.args & 2 )
    if( _mlsdb_check_index() ) return;

  if( debi.args & 4 )
    if( debi.data == NULL ) { perr("No Ptr to Data given."); return; }
}

int _m_init()
{
  /*  if( ML || DEB ) { ERR("mls/debug already initialized"); } */
  if( DEB ) return 1;

  m_init();
  DEB = m_create(100,sizeof(lst_owner));
  atexit( exit_error );
  return 0;
}

void _m_destruct()
{
  // check for allocated lists
  lst_owner *o;
  int i;

  for(i=-1; m_next(DEB,&i,&o); ) {
    if( o->allocated == 42 && o->ln > 0 ) {
      WARN("List %d still allocated. "
	   "Source: %s() in %s:%d", i+1, o->fun, o->fn, o->ln);
    }
  }
  m_free(DEB);
  m_destruct();
  debi.me = NULL;
}


int _m_create(int ln, const char *fn, const char *fun,
		int n, int w)
{
  lst_owner *lo;
  int len, m;
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,0,0,0,0 );
  m = m_create(n,w);
  len = m_len(DEB);
  if( m > len ) m_new( DEB, m-len );
  lo = (lst_owner*) mls( DEB, m-1 );
  lo->ln = ln; lo->fn = fn; lo->fun = fun;
  lo->allocated = 42;
  TRACE(1,"NEW LIST %d allocated by %s:%d in %s", m, fun, ln, fn );
  return m;
}


int _m_free(int ln, const char *fn, const char *fun,
	       int m )
{
  if( !m ) return 0;
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,1,m,0,0 );
  m_free(m);
  lst_owner *o = (lst_owner *)mls( DEB, m-1 );
  o->ln = -ln;
  o->fun = fun; o->fn = fn;
  TRACE(1,"Free List %d", m );
  return 0;
}

void* _m_buf(int ln, const char *fn, const char *fun,
	       int m )
{
  if( !m ) return 0;
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,3,m,0,0 );
  return m_buf(m);
}

void* _mls( int ln, const char *fn, const char *fun,
	    int h, int i )
{
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,3,h,i,0 );
  return mls(h,i);
}

int _m_next( int ln, const char *fn, const char *fun,
	    int h, int *i, void *d )
{
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,7,h, i ? *i : -1 , d );
  return m_next(h,i,d);
}

int _m_put( int ln, const char *fn, const char *fun,
	    int h, const void *d )
{
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,5,h,0,d );
  return m_put(h,d);
}

void _m_clear( int ln, const char *fn, const char *fun,
	      int h )
{
  _mlsdb_caller(__FUNCTION__, ln,fn,fun,1,h,0,0 );
  m_clear(h);
}


/*
   -------------------------------------------------------------------------

   UTILITY

 -------------------------------------------------------------------------
*/

#undef MLS_DEBUG_DISABLE
#include "mls.h"

void m_print_version()
{
  puts("MLS - Secure, Easy, Low-Overhead Array-Memory-Mangement");
  puts( Version );
}

void m_bzero( int m )
{
  lst_t lp =*_get_list(m);
  bzero( lp->d, lp->max * lp->w );
}




// ********************************************
//
// Helper functions, keep it simple
//
// ********************************************


//!X! speicher das char c an das ende von marray m
void m_putc(int m,char c)
{
  m_put(m,&c);
}

//!X! speicher ein int an das ende von marray m
void m_puti(int m,int c)
{
  m_put(m,&c);
}

// Vorteile:
// Übergabe eines Speicherbereichs an eine Funktion die
// ggf. den Speicher vergrößern muss:
//
// Vorher:
//
/* FILE * fp; */
/*            char * line = NULL; */
/*            size_t len = 0; */
/*            ssize_t read; */

/*            fp = fopen("/etc/motd", "r"); */
/*            if (fp == NULL) */
/*                exit(EXIT_FAILURE); */

/*            while ((read = getline(&line, &len, fp)) != -1) { */
//
//
// Nachher:
//
/* FILE * fp; */
/*            int line=m_create(100,1); */
/*            ssize_t read; */

/*            fp = fopen("/etc/motd", "r"); */
/*            if (fp == NULL) */
/*                exit(EXIT_FAILURE); */

/*            while ((read = getline(line, fp)) != -1) { */
//
//
//
// wenn der Puffer auf den "line" verweist vergrößert werden muss bleibt
// "line" unverändert.
// zudem wird durch die funktion sofort klar, das hier speicher reserviert wurde
// und am ende der funktion eine freigabe "m_free(line)" stehen muss.
//
// vorher war es notwendig zusätzlich die funktionen von getline zu kennen,
// um zu wissen das "line" ein zeiger auf einen reservierten speicherbereich
// sein kann.
//





//TODO: Erkläre Stringlist
// String-Liste
// Eine String-Liste besteht aus einer Liste von Datenblöcken beliebiger Größe.
// Jeder Datenblock der Stringliste enthält als erstes einen
// Zeiger auf einen Speicherbereich mit einem Null-terminierten String.
//
// struct my_data_st { char *string; int x, y,z; }
// mlsCreate( 100, sizeof( my_data_st ))
//
// my_data_st m; m.string = strdup("Hello"); m_put( ll, &m );
//
//


//!X!
//Loesche alle Zeilen einer String-Liste
//wird CLEAR_ONLY gesetzt werden die Inhalte
//der String-Liste geloescht sonst
//auch die Liste selber
//
void m_free_strings(int list, int CLEAR_ONLY )
{
  int index; char **strp;
  if( list < 1 ) return;
  m_foreach(list,index,strp) {
    if( *strp ) free(*strp);
    *strp=NULL;
  }
  if( CLEAR_ONLY )
    m_clear(list); // reset array size to zero
  else
    m_free(list); // free array

}

/**
 * @brief Splits the string `s` at each occurrence of the character `c` and copies the resulting substrings into the string list `m`.
 *
 * - An empty string results in an entry with a string of length zero.
 * - A string containing only the separator results in a string list with two entries, both empty.
 * - If `m` is set to 0, a new string list is created. Otherwise, the existing list is cleared and used.
 * - If `remove_wspace` is non-zero, leading and trailing whitespace characters in the resulting substrings are removed.
 *
 * @param m The string list to which the substrings are copied. If set to 0, a new list is created.
 * @param s The input string to be split.
 * @param c The character used as the delimiter for splitting the string.
 * @param remove_wspace Flag to indicate whether leading and trailing whitespace should be removed from the substrings.
 * @return The generated string list.
 */
int m_split(int m, const char *s, int c, int remove_wspace)
{
  int p=0,
    start=0,
    end;
  char *szTemp;

  if( m ) m_free_strings(m,1); else m=m_create(10,sizeof(char*));

  for(;;) {

    // leading white-space
    while( isspace(s[p]) && s[p]!=c ) p++;
    start=p;

    // delimeter
    while( s[p] && s[p] != c ) p++;

    //  trailing whitespace before delimeter, zero - length: end < start
    end=p;
    while( end>=start && isspace( s[--end] ) );

    if( end >= start )
      {
	szTemp= strndup( s+start, end-start+1 );
      }
    else
      szTemp = strdup("");
    m_put( m, &szTemp );

    if( s[p] ) p++; else break;
  }

  return m;
}

/**
 * @brief Splits the string `s` at each occurrence of the character `c` and copies the handles to the resulting substrings into the array list `m`.
 * 
 *
 * - An empty string results in an entry with a string of length zero.
 * - A string containing only the separator results in a string list with two entries, both empty.
 * - If `m` is set to 0, a new string list is created. Otherwise, the existing list is cleared and used.
 * - If `remove_wspace` is non-zero, leading and trailing whitespace characters in the resulting substrings are removed.
 *
 * @param m The string list to which the substrings are copied. If set to 0, a new list is created.
 * @param s The input string to be split.
 * @param c The character used as the delimiter for splitting the string.
 * @param remove_wspace Flag to indicate whether leading and trailing whitespace should be removed from the substrings.
 * @return The generated string list.
 */
int s_split(int m, const char *s, int c, int remove_wspace)
{
  int p=0,
    start=0,
    end;
  char *szTemp;

  if( m ) m_free_strings(m,1); else m=m_create(10,sizeof(char*));

  for(;;) {

    // leading white-space
    while( isspace(s[p]) && s[p]!=c ) p++;
    start=p;

    // delimeter
    while( s[p] && s[p] != c ) p++;

    //  trailing whitespace before delimeter, zero - length: end < start
    end=p;
    while( end>=start && isspace( s[--end] ) );

    if( end >= start )
      {
	szTemp= strndup( s+start, end-start+1 );
      }
    else
      szTemp = strdup("");
    m_put( m, &szTemp );

    if( s[p] ) p++; else break;
  }

  return m;
}





#include <regex.h>

/**
 !X!
 Match DIGIT: [[:digit:]]
 Match DIGIT AND/OR a-f or A-F: [a-f[:digit:]A-F]
 Subexpresssion-Match: (first)##*(second)
 Matches the string: first#####second and
 returns the substring-list
 "first", "second"
 Returns: StringList
 len(m) == 0 : NO MATCH
 len(m) == 1 : matched string
 len(m) > 1  : matched string and matched subexpressions
 returns:  substring-list
 restrictions:
 bei jedem lauf wird die regex kompiliert
 pos. der substrings im zu durchsuchenden str. geht verloren
 todo: bei bedarf eine find-all funktion die alle passenden
 stellen fuer ein pattern sucht


 @PARAMS: int m - eine string-liste oder 0. wird mit m_free_strings gelöscht.
 char *regex - die expression
 char *s - der zu durchsuchende string

 @RETURNS: eine stringliste

*/

int m_regex( int m, const char *regex, const char *s )
{
  char *szTemp;
  regex_t regc;
  regmatch_t *pm;
  int i,error;

  int subexp=1;
  int p=0;

  error = regcomp( &regc, regex, REG_EXTENDED );
  if( error ) ERR("REG_EXPRESSION %s not valid", regex );

  while( regex[p] ) {
    if( regex[p] == '(' ) subexp++;
    p++;
  }
  pm = (regmatch_t *)malloc( sizeof(regmatch_t) * subexp);

  if( m > 1 ) m_free_strings(m,1); else m=m_create(subexp+1,sizeof(char*));

  error = regexec( &regc, s, subexp, pm, 0 );
  if( ! error ) {
    for( i=0; i<subexp; i++ ) {
      if( pm[i].rm_so == -1 ) break;
      szTemp = strndup( s+pm[i].rm_so,
			pm[i].rm_eo - pm[i].rm_so );
      m_put(m,&szTemp);
    }
  }
  free( pm );
  regfree( &regc );
  return m;
}





//!X! Copy List m
int m_dub(int m)
{
  int r = m_create( m_len(m), m_width(m) );
  m_write( r,0, mls(m,0), m_len(m) );
  return r;
}


/*
0xxxxxxx
10111111   illegal
110xxxxx   10xxxxxx
1110xxxx   10xxxxxx 10xxxxxx
11110xxx   10xxxxxx 10xxxxxx 10xxxxxx
*/
#define UTF8GET()							\
    if( EOS() ) return -1;						\
    c = GETCH(); INC();							\
									\
    if( (c & 0x80)  == 0 ) return  c;					\
    if( (c & 0x40)  == 0 ) return 0xFFFD;				\
    if( (c & 0x20)  == 0 ) { len=1; c &= 0b00011111; goto read; }	\
    if( (c & 0x10)  == 0 ) { len=2; c &= 0b00001111; goto read; }	\
    if( (c & 0x08)  == 0 ) { len=3; c &= 0b00000111; goto read; }	\
    if( (c & 0x04)  == 0 ) { len=4; c &= 0b00000011; goto read; }	\
    if( (c & 0x02)  == 0 ) { len=5; c &= 0b00000001; goto read; }	\
    return 0xFFFD;							\
									\
 read:									\
    ret = c;								\
    while( len > 0 ) { len--;						\
	if( EOS() ) return -1;						\
	c = GETCH();							\
	if( (c & 0xc0) != 0x80 ) /* wrong header */			\
	    return 0xFFFD;						\
	INC();								\
	ret = (ret << 6) |  (c & 0x3f);					\
    }									\
									\
    return ret



/* get current char (32bit) and increment p by character length
   -1 is reserved as end of stream indicator
*/
int m_utf8char(int buf, int *p)
{
    unsigned char c;
    uint32_t ret;
    int len;

#define GETCH() (*(unsigned char*)mls(buf,(*p)))
#define EOS()   ((*p)>=m_len(buf))
#define INC()   ((*p)++)

    UTF8GET();

#undef GETCH
#undef EOS
#undef INC
}

/* get current char (32bit) and increment (*s) by character length
   -1 is reserved as end of stream indicator
*/
int utf8char(char **s)
{
    unsigned char c;
    uint32_t ret;
    int len;

#define GETCH() (**s)
#define EOS()   ((**s)==0)
#define INC()   ((*s)++)

    UTF8GET();

#undef GETCH
#undef EOS
#undef INC
}



int utf8_getchar(FILE *fp, utf8_char_t buf )
{
  int len, ch, nx, i;

 read_single:
  ch = fgetc(fp);

 parse_next_char:
  if( ch < 0x80  )  // valid char or EOF, Lower Than instead of Logic AND
    {
      len = 1;
      goto read_multi_byte;
    }

  if( (ch & 0x40) == 0 )	// invalid, discard
    {
      goto read_single;
    }
  if( (ch & 0x20) == 0 ) // Bit 7=0
    {
      len = 2;
      goto read_multi_byte;
    }

  if( (ch & 0x10) == 0 ) // Bit 6=0
    {
      len = 3;
      goto read_multi_byte;
    }

  if( (ch & 0x08) == 0 ) // 4 byte
    {
      len = 4;
      goto read_multi_byte;
    }

  if( (ch & 0x04) == 0 ) // 5 byte
    {
      len = 5;
      goto read_multi_byte;
    }

  if( (ch & 0x02) == 0 ) // 6 byte
    {
      len = 6;
      goto read_multi_byte;
    }

  // illegal char, read next
  goto read_single;

 read_multi_byte:
  buf[0] = len;
  buf[1] = ch < 0 ? 0xff : ch;
  i = 1;
  while( i++ < len )
    {
      nx = fgetc(fp); if( nx < 0 ) return -1;
      if( (nx & 0xc0) != 0x80 ) { // wrong header, discard char
	  ch=nx; goto parse_next_char;
      }
      buf[i] = nx;
    }
  return ch;
}


// lese aus fp und speicher in m, falls m>0,
// bis EOF oder das Zeichen delim gefunden wurde.
// returns: -1 EOF, delim - OK
// wird m>0 übergeben, erhält man einen mit null-terminierten string
 //
int m_fscan( int m, char delim, FILE *fp)
{
  int ch;
  utf8_char_t buf; buf[0]=0;
  for(;;) {
    ch=utf8_getchar(fp,buf);
    if( ch < 0 || ch == delim ) {
      if( m ) { buf[0] = 0; m_put( m, buf ); }
      return ch;
    }
    if( m ) m_write( m, m_len(m), buf+1, *buf );
  }
}

//
// lese aus fp und speicher in m, falls m>0,
// bis EOF oder das Zeichen delim gefunden wurde.
// returns: -1 = EOF, oder 'delim'
// wird m>0 übergeben, erhält man einen mit null-terminierten string
// führende und folgende Leerzeichen werden nicht gespeichert
// es werden doppelte leerzeichen zu einem reduziert (squeeze).
// tabs werden zu leerzeichen
// die gelesenen daten werden an m angehängt, falls m existiert (i.e. m>0)!
int m_fscan2( int m, char delim, FILE *fp)
{
  int ch;
  int IN=1; char SPACE = 0;
  utf8_char_t buf; buf[0]=0;
  for(;;) {
    ch=utf8_getchar(fp,buf);
    if( ch < 0 || ch == delim ) {
      if( m ) { buf[0] = 0; m_put( m, buf ); }
      return ch;
    }
    if( m ) {
      if( isspace(ch) ) {
	if( IN ) continue;
	if( SPACE ) continue;
	SPACE = 32; continue;
      }
      else {
	if( IN ) IN=0;
	if( SPACE && m ) m_put(m,&SPACE);
	SPACE=0;
      }
      m_write( m, m_len(m), buf+1, *buf );
    }
  }
}


//!X! vergleiche zwei marrays mit strncmp
// returns: 0 wenn beide gleich sind, sonst !=0
// beide strings müssen mit 0 enden
int m_cmp( int a, int b )
{
  int l1,l2;
  l1=m_len(a); l2=m_len(b);
  int len = Max( l2, l1 );
  return strncmp( (char*)mls(a,0), (char*)mls(b,0), len );
}


/* suche nach obj in der liste m
   ist obj noch nicht vorhanden wird es der liste hinzugefügt
   RETURNS:
   index von obj
*/
int m_lookup_obj( int m, void *obj, int size )
{
    int p; void *d;

    m_foreach(m,p,d)
	if( memcmp( d, obj, size ) == 0 ) return p;

    p = m_new(m,1);
    memcpy( mls(m,p), obj, size );
    return p;
}


//!X! lookup-table verwaltung
// suche nach key in liste m
// falls key gefunden gebe den treffer zurück
// ansonsten füge key hinzu und gebe key zurück
// m - marray [ int ]
// key - marray [ char ], type: ZMSTRING zero-terminated string in marray
// returns: entweder wird (key) zurückgegeben, dann wurde key hinzugefügt,
// oder es wird ein wert !=key zurückgegeben, dann existiert der schlüssel
// schon und der rückgabewert zeigt auf den existieren schlüssel.
//
int m_lookup( int m, int key )
{
  int p,*d;

  if( m_len(key) == 0 ) ERR("Key of zero size");
  m_foreach(m,p,d)
    if( m_cmp( *d, key ) == 0 ) return *d;

  m_put( m, &key);
  return key;
}

int m_lookup_str( int m, const char * key, int NOT_INSERT )
{
  int p; char **d;

  if( !key || strlen(key) == 0 ) ERR("Key of zero size");
  m_foreach(m,p,d) {
    if( *d == NULL ) continue;
    if( strcmp( *d, key ) == 0 ) return p;
  }

  if( NOT_INSERT ) return -1;

  p=m_new(m,1);
  *(char**) mls(m,p) = strdup(key);
  return p;
}


// ***********
//  VARIABLES
// ***********
// initalize a new set of variables
int     v_init( void )
{
  return m_create( 100, sizeof (int) );
}

// free  a  set of variables
void    v_free( int vl )
{
  int p, *d;
  m_foreach(vl,p,d)
    m_free_strings(*d,0);
  m_free(vl);
}

// using variable name

//
int v_set( int vs, const char* name, const char* value, int pos )
{
  int key = v_lookup(vs,name);
  v_kset( key, value, pos );
  return key;
}

// v_vaset( vs   [, name,value]*
// set a list of variables
void    v_vaset( int vs, ... )
{
  va_list argptr;
  char *name, *value;

  va_start(argptr, vs );

  while( (name = va_arg( argptr, char* )) != NULL )
    {
      value = va_arg( argptr, char* );
      v_set( vs, name, value, VAR_APPEND );
    }

  va_end(argptr);
}

void    v_clr( int vs, const char* name )
{
  return v_kclr( v_lookup(vs,name ) );
}


char*   v_get( int vs, const char* name, int pos )
{
  return v_kget( v_lookup(vs,name ), pos);
}


int     v_len( int vs, const char* name )
{
  return v_klen( v_lookup(vs,name ));
}



// returns index of variable "name" in list vs
int v_find_key( int vs, const char *name )
{
  char *s; int p, *d;
  m_foreach( vs, p, d ) {
    s = STR(*d,0);
    if( strcmp(s,name) == 0 ) return p;
  }
  return -1;
}

void v_remove( int vs, const char* name )
{
  int pos =v_find_key( vs,name );
  if( pos >= 0 ) {
    m_free_strings( INT(vs,pos), 0);
    m_del( vs, pos );
  }
}

/* return key to access variable "name" inside "vl" */
int v_lookup( int vl, const char* name )
{
  if( is_empty(name) ) return -1;

  int p = v_find_key( vl, name );
  if( p >= 0 ) {
    return INT(vl,p);
  }

  TRACE(1,"Create on Stack (%d) Var: %s",vl, name );
  int var=m_create(2,sizeof(char*));
  char *s=strdup(name); m_put(var, &s);
  m_put(vl,&var);
  return var;
}

// Access Stringlist Var
//
// row=0 - varname
// row=1 - first value ...
//
// row < 0 OR row==Len : Append val
// row > len : Error, exit
void    v_kset( int var, const char* v, int row )
{
  char *val=NULL;
  if( v )
      val = strdup(v);

  if( row < 0 || row >= m_len(var) ) // append-value
    {
      m_put(var,&val);
    }
  else // replace value
    {
      char **d = (char**) mls(var,row);
      if( *d ) free(*d);
      *d = val;
    }
}

void    v_kclr( int var )
{
  int i=0; char **d;
  while( m_next(var,&i,&d) )
    if( *d ) {
      free(*d); *d=NULL;
    }

  m_setlen(var,1);
}

char*   v_kget( int var, int row )
{
  if( var <=0 ) return "";
  int len=m_len(var);
  if( row >= len ) return "";
  char *s=STR(var,row);
  if( s == NULL ) return "";
  return s;
}

int v_klen( int key )
{
  return m_len(key) -1;
}


void se_init(  str_exp_t *se  )
{
  memset( se, 0, sizeof *se );
}

void se_free(  str_exp_t *se )
{
  m_free_strings( se->splitbuf,0 );
  m_free( se->values );
  m_free( se->indices );
  m_free( se->buf );
  memset( se,0,sizeof *se);
}


void se_realloc_buffers( str_exp_t *se )
{
  if( !se->buf ) {
    se->splitbuf = m_create(10,sizeof(char*));
    se->values=m_create(10,sizeof(char*));
    se->indices=m_create(10,sizeof(int));
    se->buf=m_create(100,1);
    return;
  }

  m_free_strings(se->splitbuf,1);
  m_clear(se->values); m_clear(se->indices);
  m_clear(se->buf);
  se->max_row=0;
}

// tbd
// returns:
//  1 ALL
//  2..n SINGLE
//  0 ERROR
//
static int parse_index(const char **s)
{
  int val;
  const char *p = *s;

  if( *p !='[' ) return 0;
  p++;
  if( *p == '*' && p[1] == ']') {
    *s=p+2;
    return 1;
  }

  val=0;
  while( isdigit(*p) )
    {
      val *= 10; val+= *p - '0';
      p++;
    }
  if( *p == ']' ) { *s=p+1; return val+2; }

  return 0;
}


void se_parse(str_exp_t *se, const char *frm)
{
  ASSERT( frm && se );

  se_realloc_buffers(se); // alloc, or clear buffer

  int b=se->splitbuf;
  char *cp,prev; const char *s,*s0;

  prev=0; s = frm; s0=s;

  for(;;) {

    if( *s == 0 || (*s == '$' && prev != '\\') )
      {
        // prefix ?
        if( s0 != s ) {
          cp = strndup(s0,s-s0); // copy without *s
          m_put(b,&cp);
        }
        if( *s == 0 ) break; // exit

        // cut out varname
        s0=s; //  token start (with $-prefix)
        s++; // skip leading $

        if( *s == '\'' ) { s++; } // expand with single quotes

        while( isalnum(*s) || *s=='_' ) s++; // UNTIL DELIMITER FOUND
        // copy without delimiter
        cp=strndup(s0,s-s0);
        m_put(b,&cp); m_put(se->values,&cp);

        int index = parse_index(&s);
        m_put( se->indices, &index );
        if( *s == 0 ) break; // exit

        s0=s; // s0 points to delimiter
      }
    prev = *s;
    s++;
  }
};


// str_replace(array('\\', "\0", "\n", "\r", "'", '"', "\x1a"), array('\\\\', '\\0', '\\n', '\\r', "\\'", '\\"', '\\Z')

static void repl_char(int buf, char ch )
{
  char tab[] = { '\\', '\0', '\n', '\r', '\'', '"', '\x1a' };
  char rep[] = { '\\', '0', 'n', 'r', '\'', '"', 'Z' };

  int i;
  for(i=0;i<sizeof tab; i++ )
    if( tab[i] == ch ) {
      m_putc( buf, '\\' );
      m_putc( buf, rep[i] );
      return;
    }
  m_putc( buf, ch );
}

void  escape_buf( int buf, char *src )
{
  while( *src ) repl_char(buf, *src++);
}

int escape_str( int buf, char *src )
{
  if( !buf ) buf = m_create( 100, 1); else m_clear(buf);
  escape_buf( buf, src );
  m_putc( buf, 0 );
  return buf;
}

/** @brief erzeugt aus *s einen string mit escape zeichen
    @param s2 - 0 oder mls liste
    @param s - ein string der umgewandelt werden soll
    @param quotes - falls quotes==1 werden einfache anführungszeichen um die variable gesetzt
    @return gültiger mls string (liste mit breite 1, string ohne Nullbyte)
 */
static int field_escape(int s2, char *s, int quotes)
{
  // "*s" ist der zu speichernde string
  // um das sql-kommando zu generieren werden sonderzeichen
  // "escaped". dies ist ein gutes beispiel warum die "mls"
  // speicherverwaltung vorteile bietet. der benötigte speicher
  // von mysql_escape_string muss abgeschätzt und reserviert werden.
  // die gleiche funktion in mls ist viel einfacher zu verwenden
  if( quotes ) m_putc( s2, '\'');
  escape_buf( s2, s );
  if( quotes ) m_putc( s2, '\'');
  return s2;
}


/**
    @return einen gültigen string - immer
 */
char* se_expand( str_exp_t *se, int vl, int row )
{
  int var, index;
  int p,vn; char **d, *s;
  int quotes = 0;
  m_clear(se->buf); int buf = se->buf;
  vn=0; // number of variables


  // string zusammenfügen
  // variablen werden durch ihren wert ersetzt
  // variablen werden durch ein führendes "$" erkannt
  // folgt dem $ ein "'" wird der eingesetzte wert durch "'" umschlossen
  //
  m_foreach( se->splitbuf, p,d ) {
    s = *d;

    if( *s != '$' ) { // einfacher text-baustein, nur anhängen
      m_write( buf, m_len(buf), s, strlen(s) );
    }
    else  // variable found
      {
        if( s[1] == '\'' ) { quotes=1; s++; } else quotes=0;
        var = v_lookup(vl,s+1);
        index = INT(se->indices, vn ); vn++;

        // expand var
	if( index == 1 ) { // erzeuge eine liste von werten
          field_escape(buf, STR(var,1), quotes);
          for( index=2; index < m_len(var); index++ ) {
	    m_putc(buf, ',' );
            field_escape(buf, STR(var,index), quotes);
          }
	}
	else { // index != 1  i.e. not expand all i.e. index != [*]
          if( index == 0 ) index=row; // falls kein index angegeben wurde, benutze (row)
          else index -= 2;

	  if( index < v_klen(var) ) field_escape(buf, STR(var,index+1), quotes);
	}
      } // variable expandiert
  }

  m_putc( buf, 0 );
  return   mls( buf, 0 );
}


/** expandiert den string frm mit den variablen aus vl
 * @return	einen Zeiger auf den expandierten string
 *		dieser string wird auch als variable
 *		unter dem namen se_string in vl gespeichert
 */
char*   se_string( int vl, const char *frm )
{
  str_exp_t se;
  int data;

  se_init( &se );
  se_parse( &se, frm );
  se_expand( &se, vl, 0 );
  data = v_set( vl, "se_string", mls(se.buf,0), 1 );
  se_free(&se);
 return STR(data,1);
}

/* returns: length of string */
int s_strlen(int m)
{
    int p = m_len(m);
    return p && CHAR(m,p-1)==0 ? p-1 : p;
}

/** append cstring to mstr
 * returns: mstr
 */
int s_app1( int m, char *s )
{
    int p = s_strlen(m);
    m_write( m, p, s, strlen(s)+1 );
    return m;
}

static int vas_app(int m, va_list ap)
{
    char *name;
    while( (name = va_arg( ap, char* )) != NULL )
    {
	s_app1( m, name );
    }
    return m;
}

/** anhängen der char* strings an |m| */
int s_app(int m, ...)
{
    va_list ap;
    if(!m) m=m_create(10,1);
    va_start(ap,m);
    vas_app(m,ap);
    va_end(ap);
    return m;
}

int vas_printf(int m, int p, char *format, va_list ap )
{
    int len;
	va_list copy;


	// Patch für 64Bit machines 08.10.14
	va_copy(copy, ap);

    len=vsnprintf(0,0, format, ap ); /* get string size */
    len++;                           /* with terminating zero */
    if( m == 0 ) {
	m=m_create(len,1);
        p=0;
    }

    if( p < 0 || p > m_len(m) ) /* append to string */
      p = s_strlen(m);

    m_setlen( m, p + len );
    void *buf = mls(m, p);



	vsnprintf( buf,len, format, copy ); /* len is (stringsize + 1) */
	va_end(copy);
    return m;
}

/* string printf
   place string at p into array m
   if p<0 append str to m
   if m == 0 create new str
*/
int s_printf(int m, int p, char *format, ...)
{
    va_list ap;
    va_start(ap,format);
    m = vas_printf( m,p, format, ap );
    va_end(ap);
    return m;
}

/** das letzte zeichen des strings finden
    falls der string leer ist oder nur NULL enthält
    wird 0 zurückgegeben
*/
int s_lastchar(int m)
{
  int len = m_len(m);
  if( len == 0 ) return 0;

  do {
    len--;
  } while( len > 0 && CHAR(m,len) == 0 );

  return CHAR(m,len);
}

/* erzeuge eine kopie eines teil-strings */
int s_copy( int m, int first_char, int last_char  )
{
    if( last_char < 0 ) last_char=m_len(m)-1;
    if( first_char < 0
        || first_char > last_char
        || first_char >= m_len(m) ) return m_create(1,1);
    int size = last_char - first_char +1;
    if( first_char + size > m_len(m) )
        size = m_len(m) - first_char;
    int ret = m_create(size,1);
    m_write( ret, 0, mls(m, first_char), size );
    if( CHAR(ret, m_len(ret)-1 ) != 0 ) m_putc(ret,0);
    return ret;
}



void m_qsort( int list, int(*compar)(const void *, const void *))
{
  qsort( m_buf(list), m_len(list), m_width(list), compar );
}

int m_bsearch( const void *key, int list, int (*compar)(const void *, const void *))
{
    if( list < 1 || m_len(list) == 0 ) return -1;
    void *res = bsearch(key, m_buf(list), m_len(list), m_width(list), compar );
    if( res )
	return (res - m_buf(list)) / m_width(list);
    return -1;
}


int m_lfind(const void *key, int list, int (*compar)(const void *, const void *)){
    size_t max;
    if( list < 1 || m_len(list) == 0 ) return -1;
    max = m_len(list);
    void *res = lfind(key, m_buf(list), &max, m_width(list), compar );
    if( res )
	return (res - m_buf(list)) / m_width(list);
    return -1;
}

/** @brief insert *data into sorted list buf
 * @returns: position of new element, or ret=-pos-1 (ret<0) if elem. exists 
 */
int m_binsert( int buf, const void *data, int (*cmpf) (const void *a,const void *b ), int with_duplicates )
{
    int left = 0;
    int right = m_len(buf)+1;
    int cur = 1;
    void *obj;
    int cmp;

    if( m_len(buf)==0 ) {
	m_put(buf,data);
	return 0;
    }
    
    while(1) {
	cur = (left+right) / 2;
	obj = mls( buf, cur - 1 );
	cmp = cmpf( data, obj );
	if( cmp == 0 ) {
	  if( ! with_duplicates ) return -cur;
	  break;
	}
	if( cmp < 0 ) {
	    right=cur;
	    if( left+1 == right ) break;
	} else {
	    left  = cur;
	    if( left+1 == right ) {
		cur++;
		break;
	    }
	}
    }
    
    cur--;
    m_ins( buf, cur, 1 );
    m_write( buf, cur, data, 1 );
    return cur;
}


int s_index( int buf,int p, int ch )
{
  unsigned char *d;
  while( p<m_len(buf) ) {
    d = mls(buf,p);
    if( *d == ch ) return p;
    p++;
  }
  return -1;
}

lst_t *exported_get_list(int r)
{
    return _get_list(r);
}


/* ringbuf */
/*
  Array:
  l         : Wr
  d[0]      : Rd
  d[1..max] : data

  Rd < 0    : Empty
  Wr == Rd  : Full
  Wr zeigt immer auf einen freien Platz, solange Wr != Rd
  Rd zeigt immer auf das zu lesende Element, soland Rd > 0
*/
/* RD = -1:  empty */
/* RD == WR: full  */
int ring_create( int size )
{
    int r = m_create( size+1, sizeof(int));
    lst_t *lp =_get_list(r);
    int *rd = lst_peek(*lp,0);
    int *wr = & (*lp)->l;
    *rd = -1;
    *wr = 1;
    return r;
}

int ring_empty(int r)
{
    lst_t *lp =_get_list(r);
    int *rd = lst_peek(*lp,0);
    return( *rd < 0 );
}



int ring_full(int r)
{
    lst_t *lp =_get_list(r);
    int *rd = lst_peek(*lp,0);
    int *wr = & (*lp)->l;
    return (*rd == *wr);
}


/* if RD == WR return -1 */
int ring_put( int r, int data )
{
    lst_t *lp =_get_list(r);
    int *rd = lst_peek(*lp,0);
    int *wr = & (*lp)->l;
    int max = (*lp)->max;
    int *d  = lst_peek(*lp,*wr);

    if( *rd == *wr ) return -1; /* full */
    *d = data;
    if( *rd < 0 ) *rd = *wr; /* was empty */
    (*wr)++; if( *wr >= max ) *wr = 1;
    return 0;
}

int ring_get( int r )
{
    lst_t *lp =_get_list(r);
    int *rd = lst_peek(*lp,0);
    int *wr = & (*lp)->l;
    int max = (*lp)->max;


    if( *rd < 0 ) return -1; /* empty */
    int *d  = lst_peek(*lp,*rd);
    (*rd)++; if( *rd >= max ) *rd = 1;
    if( *rd == *wr ) *rd = -1;
    return *d;
}

void ring_free(int r )
{
    m_free(r);
}

/* return 0 if string m[p..] == s 
   else return <0 or >0
 */
int mstrcmp(int m,int p, const char *s)
{
  int res = 1;
  if( !s ) return 1;
  while( p < m_len(m) ) {
    res = CHAR(m,p) - *s;
    if( res ) break;
    if( *s == 0 ) return 0;
    p++; s++;
  }
  return res;
}

int mstr_to_long(int buf, int *p, long int *ret_val)
{
    int sign=0;
    int ch;
    long int val = 0;
    int pp = 0;
    if( !p ) p=&pp;

    if( buf <= 0 || *p < 0 || *p >= m_len(buf) ) return -1;

    while( isspace(ch=CHAR(buf,*p)) ) {
        (*p)++;
        if( *p >= m_len(buf) ) return -1;
    }

    if(! isdigit(ch) ) {
        if( ch == '-' )  { sign = -1; (*p)++; }
        else if( ch == '+' )  (*p)++;
        else return -1;
        if( *p >= m_len(buf) ) return -1;
        ch=CHAR(buf,*p);
        if(! isdigit(ch) ) return -1;
    }
    ch -= '0';

    while(1) {

        val += ch;
        (*p)++;

        if( *p >= m_len(buf) ) break;
        ch=CHAR(buf,*p);
        if(! isdigit(ch) ) break;
        ch -='0';
        if( val > LONG_MAX / 10 ) return -2 + sign;
        val *= 10;
        if( val > (LONG_MAX - ch) ) return -2 + sign;
    }

    *ret_val = sign ?  -val : val;
    return 0;
}
