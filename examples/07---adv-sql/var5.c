/*

   storage:

   VAR_CALLBACK		structs with callback pointer
			one per variable
			primary-key: var-id

   VAR_IF		structs with interface functions
			one per variable-type
			primary-key: index

   MVAR_MEM		pointer to variables
			primary-key: (group,name)
			        key: index (var-id)

   Each variable has two attributes, name and group.
   Together they form a unique identifier.

   They can be used to form a graph:
   tree: list of (node1,node2)

   node1 = group
   node2 = var-id

   A leave node is a node with arbitrary type
   A branch starts at a node with type VSET


            (1:(root,0) )

       (2:(n1,1))     (4:(n3,1))

       (3:(n2,2))

       root,n1 - must have type VSET (or INTEGER)
       n2,n3   - type is arbitrary


   mvar_parse can be used to create/access the tree.
   example:
      node = mv_parse( *root.n1.n2, "STRING" )
      - this call will construct three nodes (root,n1,n2)

      node = mv_parse( *root.n3, "INTEGER" )
      - this call will construct one node (n3)

   The complete tree is accessable with the id of the
   root-node
   to query the root id you have multiple possibilities:
   id = mv_lookup( 0, "root", NULL );
   id = mv_lookup_path( "#0.root", NULL );
   id = mv_lookup_path( "root", NULL );
   id = mv_parse( "root", NULL );
   id = mv_parse( "*root", NULL );



   var5 interface

   types:
     VAR_INTEGER
     VAR_STRING
     VAR_VSET

   init
     mvar_init

   create anon variables:
     int id = mvar_anon(0, VAR_VSET );


   create anon vset variables:
     int  g = mvar_vset();

   find/create a variable
     int q = mvar_parse( int var_path_string, int type );

     int q =  mvar_lookup( group,sname,stype );
     int q =  mvar_lookup_path(mp,stype);
     int offs = mvar_parse_path(mp,&group);
     mvar_free(q);


   read/write
     mvar_put_string(q,s,p)
     mvar_get_string(q,p)
     mvar_put_integer(q,v,p)
     mvar_get_integer(q,p)

   inspect
     mvar_type(q)
     mvar_group(q)
     mvar_name(q)
     mvar_id(q)
     mvar_path(q)
     mvar_length(q)

   signals
     var_set_callback( q1, cb1, ctx, 0 )
     var_call_callbacks( q1 )
     var_callback_destroyv



 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mls.h"
#include "ctx.h"
#include "var5.h"
#include "m_tool.h"
#include <errno.h>

#define HASH_SIZE (4*8)
#define HASH_BITS 14
#define HASH_TABLE_SIZE (1<<HASH_BITS)
#define HASH_TABLE_MASK (HASH_TABLE_SIZE-1)
int HASH_TABLE = 0;
int var5_lookup( int group, char *name, int  typeid );
void var5_delete_hash( var_t *v );


struct var_st;
typedef struct var_st var_t;

struct var_if {
    int    (*put_string) ( var_t *v, char *s, int p );
    char*  (*get_string) (var_t *v, int p);
    int    (*put_integer) ( var_t *v, long val, int p );
    long   (*get_integer) (var_t *v, int p);
    int    (*to_string) (var_t *v, int buf, int p);
    int    (*length)  (var_t *v);
    var_t* (*create)  (void);
    void   (*destroy) (var_t *v);
    void   (*clear) (var_t *v);
};


void set_error(var_t *v, char *s ) {
};

int VAR_CALLBACK = 0;
struct var_callback {
    int var_id;
    int signal, locked;
};

struct var_signal {
    void *d;
    varsig_t fn;
};

int callback_lookup_pos( int q )
{
    if(!VAR_CALLBACK) VAR_CALLBACK=m_create(1,sizeof(struct var_callback));
    int p;
    struct var_callback *cb;
    m_foreach(VAR_CALLBACK,p,cb) {
	if( cb->var_id == q ) return p;
    }
    cb = m_add(VAR_CALLBACK);
    cb->var_id = q;
    cb->signal=0; cb->locked=0;
    return m_len(VAR_CALLBACK)-1;
}

struct var_callback * callback_lookup( int q )
{
    int p = callback_lookup_pos( q );
    return mls(VAR_CALLBACK,p);
}

void var_set_callback( int q, varsig_t fn, void *d, int remove )
{
    struct var_callback *ent = callback_lookup(q);
    int p;
    struct var_signal *sig;

    if(!ent->signal) ent->signal = m_create( 1, sizeof(struct var_signal));

    /* finde signal und falls remove==TRUE entfernen */
    m_foreach( ent->signal, p, sig ) {
        if( sig->fn == fn && sig->d == d ) {
            if( remove ) m_del(ent->signal,p);
            return;
        }
    }
    if( remove ) return;

    /* signal noch nicht vorhanden, remove==FALSE, jetzt hinzufügen */
    sig = m_add(ent->signal);
    sig->d = d;
    sig->fn = fn;
}

void var_call_callbacks( int q, int s )
{
    struct var_callback *ent = callback_lookup(q);
    if(! ent->signal ) return;

    /* um rekursion zu verhindern wird signal blockiert */
    if( ent->locked ) return;
    ent->locked = 1;

    int p;
    struct var_signal *sig;
    m_foreach( ent->signal, p, sig ) {
	if( sig->fn ) sig->fn( sig->d, q, s );
    }
    ent->locked = 0;
}

void var_callback_destroy(void)
{
    int p;
    struct var_callback *ent;
    m_foreach( VAR_CALLBACK, p, ent ) {
	if( ent->signal ) m_free(ent->signal);
    }
    m_free(VAR_CALLBACK);
    VAR_CALLBACK=0;
}


/* same as var_if */
struct string_var_if {
    struct var_if;
};

/* same as var_st but with extensions */
struct string_var_st {
    struct var_st;
    int lst;
};
typedef struct string_var_st string_var_t;

/* later
char *string_get_string(var_t *v, int p)
{
    if( v->var_if != VAR_STRING ) {
	set_error(v, "this isn't a string class");
	return "";
    }
    string_var_t *s = (string_var_t*)v;
    return s->fn->core.get_string(v,p);
}

int string_put_string(var_t *v, char *str, int p)
{
    if( v->fn->if_type != VAR_STRING ) {
	set_error(v, "this isn't a string class");
	return -1;
    }
    string_var_t *s = (string_var_t*)v;
    return s->fn->core.put_string(v,str,p);
}
*/

var_t *string_impl_create(void);
void  string_impl_destroy(var_t *v);
char *string_impl_get_string(var_t *v, int p);
int string_impl_put_string(var_t *v,char *str, int p);
int string_impl_length(var_t *v);
void string_impl_clear(var_t *v);

struct string_var_if STRING_VAR_IF = {
    .put_string = string_impl_put_string,
    .get_string = string_impl_get_string,
    .length     = string_impl_length,
    .create = string_impl_create,
    .destroy = string_impl_destroy,
    .clear   = string_impl_clear
};

int string_impl_length(var_t *v)
{
    string_var_t *s = (string_var_t*)v;
    return m_len(s->lst);
}

var_t *string_impl_create(void)
{
    string_var_t *s = calloc(1, sizeof(string_var_t) );
    s->lst = m_create(2,sizeof(char*));
    return (var_t*) s;
}

void string_impl_destroy(var_t *v)
{
    string_var_t *s = (string_var_t*)v;
    m_free_strings(s->lst,0);
    free(v);
}

char *string_impl_get_string(var_t *v, int p)
{
    string_var_t *s = (string_var_t*)v;
    return v_kget(s->lst,p);
}

int string_impl_put_string(var_t *v,char *str, int p)
{
    string_var_t *s = (string_var_t*)v;
    v_kset(s->lst,str,p);
    return 0;
}

void  string_impl_clear(var_t *v)
{
    string_var_t *s = (string_var_t*)v;
    m_free_strings( s->lst, 1);
}

/*-----------------------------------------------------------------------*/

/* same as var_if */
struct estr_var_if {
    struct var_if;
};

/* same as var_st but with extensions */
struct estr_var_st {
    struct var_st;
    mvar_str_t estr;
    int watches;
    char *str_copy;
    int cache_dirty;
    int cache_index;
    int max_index;
};

typedef struct estr_var_st estr_var_t;

var_t *estr_impl_create(void);
void  estr_impl_destroy(var_t *v);
char *estr_impl_get_string(var_t *v, int p);
int estr_impl_put_string(var_t *v,char *str, int p);
int estr_impl_length(var_t *v);
void estr_impl_clear(var_t *v);

struct estr_var_if ESTR_VAR_IF = {
    .put_string = estr_impl_put_string,
    .get_string = estr_impl_get_string,
    .length     = estr_impl_length,
    .create = estr_impl_create,
    .destroy = estr_impl_destroy,
    .clear   = estr_impl_clear
};

int estr_impl_length(var_t *v)
{
    estr_var_t *s = (estr_var_t*)v;
    int len = m_len(s->estr.buf);
    if( len ) len--;
    return len;
}

static void estr_var_changed(void*ctx,int var,int sig)
{
    estr_var_t *s = (estr_var_t*)ctx;
    s->cache_dirty = 1;
}

static void estr_watches_remove( estr_var_t *s )
{
    int p,*k;
    m_foreach( s->watches, p, k ) {
	mvar_set_callback( *k, estr_var_changed, s, 1 );
    }
    m_clear(s->watches);
}


var_t *estr_impl_create(void)
{
    estr_var_t *s = calloc(1, sizeof(estr_var_t) );
    mvar_str_init(  &s->estr );
    s->watches=m_create(2,sizeof(int));
    s->cache_dirty = 1;
    return (var_t*) s;
}

void estr_impl_destroy(var_t *v)
{
    estr_var_t *s = (estr_var_t*)v;
    mvar_str_free( & s->estr );
    free(s->str_copy);
    estr_watches_remove(s);
    m_free(s->watches);
    free(v);
}


/* alloc callbacks and find longest array in used variables */
static void estr_realloc_watches(  estr_var_t *s )
{
    var_t *v = (var_t*) s;
    int group = v->group;
    mvar_str_t *se = &s->estr;
    int p; char **d;

    estr_watches_remove(s);
    s->max_index = 0;

    m_foreach( se->splitbuf, p,d ) {
	char *str = *d;
	if( *str != '$' ) continue;
	str++; if( *str == '\'' ) str++;
	int q = mvar_lookup( group, str, -1 );
	if(q<0) continue;
	mvar_set_callback(q,estr_var_changed, s, 0 );
	m_put(s->watches,&q);
	int k = mvar_length( q );
	if( k > s->max_index ) s->max_index = k;
    }
}

/* only array index has changed, no need to rebuild the parser-buffer */
static void estr_update_array( estr_var_t *s, int p )
{
    mvar_str_t *se = &s->estr;
    var_t *v = (var_t*) s;
    int group = v->group;
    int prefix = s_printf(0,0,"#%d", group );
    mvar_str_expand( se, m_str(prefix), p );
    m_free(prefix);
    s->cache_dirty = 0;
    s->cache_index = p;
}


static void estr_build( estr_var_t *s, int p )
{
    mvar_str_t *se = &s->estr;
    char *pattern  = s->str_copy;
    var_t *v = (var_t*) s;
    int group = v->group;
    int prefix = s_printf(0,0,"#%d", group );

    mvar_str_realloc_buffers( se );
    mvar_str_parse( se, pattern );
    mvar_str_expand( se, m_str(prefix), p );
    estr_realloc_watches( s );
    m_free(prefix);
    s->cache_dirty = 0;
    s->cache_index = p;
}

char *estr_impl_get_string(var_t *v, int p)
{
    estr_var_t *s = (estr_var_t*)v;
    if( s->cache_dirty ) {
	estr_build(s, p);
    }

    /* array handling
       -1 last element
       -2 2nd last ...
       0 first
       1 2nd ..
     */
    if( p ) {
	if( p >= s->max_index ) return "";
	if( p < 0 ) p += s->max_index;
	if( p < 0 ) return "";
    }

    if( s->cache_index != p ) {
	estr_update_array(s,p);
    }

    return m_str(s->estr.buf);
}

int estr_impl_put_string(var_t *v,char *str, int p)
{
    estr_var_t *s = (estr_var_t*)v;
    mvar_str_realloc_buffers( & s->estr );
    free( s->str_copy );
    s->str_copy = strdup(str);
    return 0;
}

void  estr_impl_clear(var_t *v)
{
    estr_var_t *s = (estr_var_t*)v;
    free( s->str_copy ); s->str_copy=0;
    mvar_str_realloc_buffers( & s->estr );
}

/*-----------------------------------------------------------------------*/



/* same as var_if */
struct integer_var_if {
    struct var_if;
    char buffer[100];
};

/* same as var_st but with extensions */
typedef
struct integer_var_st {
    struct var_st;
    int value;
} integer_var_t;

var_t* integer_impl_create(void);
void   integer_impl_destroy(var_t *v);
char*  integer_impl_get_string(var_t *v, int p);
int    integer_impl_put_string(var_t *v,char *str, int p);
long   integer_impl_get_integer(var_t *v, int p);
int    integer_impl_put_integer(var_t *v, long val, int p);
int    integer_impl_length(var_t *v);

struct integer_var_if INTEGER_VAR_IF = {
    .put_string = integer_impl_put_string,
    .get_string = integer_impl_get_string,
    .put_integer = integer_impl_put_integer,
    .get_integer = integer_impl_get_integer,
    .length      = integer_impl_length,
    .create = integer_impl_create,
    .destroy = integer_impl_destroy
};

var_t *integer_impl_create(void)
{
    integer_var_t *s = calloc(1, sizeof(integer_var_t) );
    s->value=m_create(1,sizeof(long));
    return (var_t*) s;
}

int integer_impl_length(var_t *v)
{
    integer_var_t *s = (integer_var_t*)v;
    return m_len(s->value);
}

void integer_impl_destroy(var_t *v)
{
    integer_var_t *s = (integer_var_t*)v;
    m_free(s->value);
    free(s);
}

char *integer_impl_get_string(var_t *v, int p)
{
    static char buffer[100];
    long val = integer_impl_get_integer(v,p);
    snprintf( buffer, sizeof(buffer),
	      "%ld", val );
    return buffer;
}

int integer_impl_put_string(var_t *v,char *str, int p)
{
	errno = 0;    /* To distinguish success/failure after call */
	char *endptr;
	long val = strtol(str, &endptr, 0);	
        /* Check for various possible errors */
	if (errno != 0 || *endptr != 0 ) {
		WARN("type error; got string but expected integer");
	}
	return integer_impl_put_integer(v,val,p);
}

int integer_impl_put_integer(var_t *v, long value, int p)
{
    integer_var_t *s = (integer_var_t*)v;
    if( p < 0 ) {
	m_put(s->value,&value);
	return 0;
    }
    if( p >= m_len(s->value) ) m_setlen(s->value,p+1);
    *(long *)mls(s->value,p)=value;
    return 0;
}

long integer_impl_get_integer(var_t *v, int p)
{
    integer_var_t *s = (integer_var_t*)v;
    if( p >= m_len(s->value) ) return -1;
    return *(long *)mls(s->value,p);
}

/* ---------------------------------------------------------------------*/

void  vset_impl_destroy(var_t *v);
var_t *vset_impl_create(void);
char *vset_impl_get_string(var_t *v, int p);

void mvar_free(int p);
int mvar_path(int id, int mp);

#define vset_var_if integer_var_if

/* same as integer_var_st  */
typedef
struct vset_var_st {
    struct integer_var_st;
} vset_var_t;

struct vset_var_if VSET_VAR_IF = {
    .put_string = integer_impl_put_string,
    .get_string = vset_impl_get_string,
    .put_integer = integer_impl_put_integer,
    .get_integer = integer_impl_get_integer,
    .length = integer_impl_length,
    .create =  integer_impl_create,
    .destroy = vset_impl_destroy,
 };

char *vset_impl_get_string(var_t *v, int p)
{
    static char buf[100];
    integer_var_t *s = (integer_var_t*)v;
    if( p >= m_len(s->value) ) return "";
    int x = *(long *)mls(s->value,p);
    int str = mvar_path(x,0);
    strncpy(buf,m_str(str), sizeof(buf));
    return buf;
}

void  vset_impl_destroy(var_t *v)
{
    long *d; int p;
    integer_var_t *s = (integer_var_t*)v;
    m_foreach(s->value,p,d) {
	mvar_free(*d);
    }
    m_free(s->value);
    free(s);
}

/* -----------------------------------------------------------*/

/* --------------------------------------------------------*/
struct var_if *vreg_getif(uint8_t id);

struct var_if *var_getif(var_t *v)
{
    return vreg_getif( v->var_if );
}


var_t *var_create( void *dfn )
{
    struct var_if *fn = dfn;
    return fn->create();
}

void var_destroy(var_t*v)
{
    var_getif(v)->destroy(v);
}

void var_clear( var_t *v )
{
    struct var_if *fn = var_getif(v);
    if( fn->clear ) fn->clear(v);
}

int var_put_string( var_t *v, char *s, int p )
{
    struct var_if *fn = var_getif(v);
    return fn->put_string ? fn->put_string(v,s,p) : -1;
}
char* var_get_string( var_t *v, int p )
{
    struct var_if *fn = var_getif(v);
    return fn->get_string ? fn->get_string(v,p) : "";
}
long var_get_integer( var_t *v, int p )
{
    struct var_if *fn = var_getif(v);
    return fn->get_integer ? fn->get_integer(v,p) : -1;
}
int var_put_integer( var_t *v, long val, int p )
{
    struct var_if *fn = var_getif(v);
    return fn->put_integer ? fn->put_integer(v,val,p) : -1;
}
int var_length( var_t *v )
{
    struct var_if *fn = var_getif(v);
    return fn->length ? fn->length(v) : -1;
}

static int VAR_IF = 0;
struct var_register_st {
    char *name;
    struct var_if *fn;
};

struct var_register_st *vreg_get(int id)
{
    return mls(VAR_IF,id);
}


int  mvar_registry( void *funcs, char *name, int id )
{
    if( !VAR_IF ) VAR_IF=m_create(10,sizeof(struct var_register_st));
    if( id >= m_len(VAR_IF)) m_setlen(VAR_IF, id+1);
    struct var_register_st *v = vreg_get(id);
    v->name = strdup(name);
    v->fn = funcs;
    return id;
}

int  mvar_registry_new( void *funcs, char *name )
{
    int id = m_len(VAR_IF);
    return mvar_registry( funcs,name,id);
}


void var_register_destroy(void)
{
    int p; struct var_register_st *v;
    m_foreach(VAR_IF,p,v) { free(v->name); }
    m_free(VAR_IF);
}

int vreg_ifnum(char *name)
{
    return m_lookup_str(VAR_IF,name,1);
}

// find interface by id
struct var_if *vreg_getif(uint8_t id)
{
    return vreg_get(id)->fn;
}

// find interface by name
struct var_if *var_register_lookup(char *name)
{
    int p=vreg_ifnum(name);
    if( p < 0 )
	ERR("could not find registerd class %s", name );
    return vreg_getif(p);
}

char* mvar_name_of_type( int id )
{
    return vreg_get(id)->name;
}


static int MVAR_MEM = 0;
static int MVAR_FREE = 0;

var_t **mvar_get(int p )
{
    ASERR(p>0,"var <=0: %d", p);
    return mls(MVAR_MEM,p -1);
}

int mvar_alloc( void )
{
    if( !MVAR_MEM ) {
	MVAR_MEM = m_create( 10, sizeof(var_t*) );
	MVAR_FREE = m_create(10, sizeof(int) );
    }
    int *id = m_pop(MVAR_FREE);
    if(id) return *id;
    return m_new(MVAR_MEM,1) +1;
}

void mvar_create_var( int id, int ifnum )
{
    ASSERT( id > 0 );
    var_t **v = mvar_get(id);
    if( ifnum < 0 ) ERR("interface %d not defined", ifnum );
    *v = var_create(vreg_getif(ifnum) );
    (*v)->var_if = ifnum;
    TRACE(1,"new var %d : %s", id,  mvar_name_of_type(ifnum) );
}

/* returns number of var +1, because that garanties
   that var-number is never zero
   maybe a bad idea
*/
int mvar_create( int id )
{
    int p = mvar_alloc();
    mvar_create_var( p, id );
    return p;
}

/* free mvar
   - remove from hash table
   - remove from mvar_mem
   - put id in MVAR_FREE for reuse of this slot
*/
void mvar_free( int id )
{
    if( id <=0 ) return;

    var_t **v = mvar_get(id);
    if( ! *v ) {
	WARN("double-free Var %d", id);
	return;
    }
    TRACE(2, "FREE %s", (**v).name );
    var5_delete_hash( *v );
    var_destroy( *v );
    *v=0;
    m_put(MVAR_FREE, &id );
}

static void mvar_free_all(void)
{
    int p = MVAR_MEM ? m_len(MVAR_MEM) : 0;
    while( p ) {
	if( *mvar_get(p) ) mvar_free(p);
	p--;
    }

    m_free(MVAR_FREE);
    m_free(MVAR_MEM);
    MVAR_MEM=0;
    MVAR_FREE=0;
}

void mvar_clear( int id )
{
    if( id == 0 ) return;
    var_t *v =  *mvar_get(id);
    if( v != NULL ) var_clear( v );
}


int mvar_put_string( int id, char *s, int p )
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? -1 : var_put_string( v, s, p );
}

char* mvar_get_string( int id, int p )
{
    if( id == 0 ) return "";
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? "" : var_get_string( v, p );
}

int mvar_put_integer( int id, long s, int p )
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? -1 : var_put_integer( v, s, p );
}

long mvar_get_integer( int id, int p )
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? -1 : var_get_integer( v, p );
}

int mvar_type(int id)
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? -1 : v->var_if;
}

int mvar_group(int id)
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? -1 : v->group;
}

/* dangerous - could be not null terminated */
char* mvar_name(int id)
{
    if( id == 0 ) return "";
    var_t *v =  *mvar_get(id);
    return ( v == NULL ) ? "" : v->name;
}

int mvar_length(int id)
{
    if( id == 0 ) return -1;
    var_t *v =  *mvar_get(id);
    return  ( v == NULL ) ? -1 : var_length( v );
}

int mvar_path(int id, int mp)
{
    char *s = mvar_name(id);
    int g =  mvar_group(id);
    return s_printf(mp,0,"#%u.%.28s", g,s );
}


// extract group number from  variable names in the form of "#9999.myname"
// parse a group number with '#' prefix and trailing '.' (dot)
// set *group to the parsed number
// return index of character after trailing dot.
int mvar_parse_path(int mp, int *group)
{
    *group=0;
    if( mp <=0 ) return -1;
    if( CHAR(mp,0) != '#' ) return 0;
    char *endp;
    *group = (int) strtoul(mls(mp,1),&endp,10);
    if(! endp || *endp != '.' ) return -1;
    return ( endp - m_str(mp) + 1 );
}


static int next_dot(int m, int *p, int w)
{
    int l = m_len(m);
    while( *p < l ) {
	int ch =  CHAR(m,*p);
	(*p)++;
	if( !ch || ch  ==  '.' ) {
	    m_putc(w, 0);
	    return ch;
	}
	m_putc(w,ch);
    }
    return -1;
}



/* multi-parser

   form 1) g1.g2.g3.var
   form 2) #9999.var

   specify a variable like a x11 resource
   each component will get type VSET
   last component will get given type
*/
int mvar_parse( int mp, int type_id  )
{
    int ch = CHAR(mp,0);
    if( ch == '#' ) return mvar_lookup_path(mp,type_id);

    /* multi-point-parser */
    int id    =  0;
    int group =  0;
    int start =  0;
    int w     =  m_create(20,1);
    int t     =  VAR_VSET;

    if( ch == '*' ) {	       /*  skip asterix and following dot -- legacy syntax  */
	start = CHAR(mp,1) == '.' ? 2 : 1;
    }

    do {
	m_clear(w);
	ch = next_dot( mp, &start, w );
	if( ch < 0 ) ERR("error parsing %s", m_str(mp));
	if( ch == 0 ) t = type_id;
	group = id;
	id = mvar_lookup(group, m_str(w), t);

    } while( ch );
    m_free(w);
    return id;
}


int mvar_parse_string(const char *s, int type_id )
{
    int name = s_printf(0,0, (char*)s );
    int key = mvar_parse( name, type_id );
    m_free(name);
    return key;
}



/* suche nach einer variable mit name (group,name)
   wird die variable gefunden wird ihre ID übergeben.
   wird die variable nicht gefunden und der typename
   ist nicht gesetzt wird -1 übergeben

   wird die variable nicht gefunden und der typename
   ist gesetzt wird eine neue variable angelegt:

   falls group==0 var_create
   sonst var_create und der variable group die id hinzufuegen
   die var mit der der nummer x wird im
   array MVAR_MEM ab position (x-1) gespeichert!


*/
int mvar_lookup( int group, char *name, int type_id )
{
    int key = var5_lookup( group,name,type_id );
    if(key < 0 && type_id >= 0) {
	ERR("INTERNAL ERROR: could not create a var with name: %s and  type: %d", name, type_id );
    }
    return key;
}

/* create a new anon-variable, t must be specified */
int mvar_anon( int g, int t )
{
    ASERR( t>=0, "type must be specified" );
    /* make it easier to create unique names */
    static unsigned long n = 0;
    char buf[29] = { 0 };
    /* create a unique name - should never iterate :-) */
    do {
	n++;
	sprintf(buf, "€%lx", n );
    } while( mvar_lookup( g, buf, -1 ) > 0 );

    return mvar_lookup(g,buf,t);
}

int mvar_vset(void)
{
    return mvar_anon(0,VAR_VSET);
}

int mvar_lookup_path( int mp, int  t )
{
    int p,g;
    p=mvar_parse_path(mp,&g);
    if( p < 0 ) return -1;
    return mvar_lookup(g, mls(mp,p), t);
}

void mvar_destruct(void)
{
    mvar_free_all();
    var_register_destroy();
    var_callback_destroy();
    m_free_list(HASH_TABLE);
}


void mvar_init(void)
{
    mvar_registry( &ESTR_VAR_IF, "ESTR", VAR_ESTR );
    mvar_registry( &STRING_VAR_IF, "STRING", VAR_STRING );
    mvar_registry( &INTEGER_VAR_IF, "INTEGER", VAR_INTEGER );
    mvar_registry( &VSET_VAR_IF, "VSET", VAR_VSET );
}

/*        -------------------------------------------------------------------------------------       */
/* var5 hashmap ------------------------------------------------------------------------------------- */
// Dedicated to Pippip, the main character in the 'Das Totenschiff'
// roman, actually the B.Traven himself, his real name was Hermann
// Albert Otto Maksymilian Feige.  CAUTION: Add 8 more bytes to the
// buffer being hashed, usually malloc(...+8) - to prevent out of
// boundary reads!  Many thanks go to Yurii 'Hordi' Hordiienko, he
// lessened with 3 instructions the original 'Pippip', thus:
#define _PADr_KAZE(x, n) ( ((x) << (n))>>(n) )
static uint32_t FNV1A_Pippip_Yurii(const char *str, size_t wrdlen)
{
	const uint32_t PRIME = 591798841; uint32_t hash32; uint64_t hash64 = 14695981039346656037UL;
	size_t Cycles, NDhead;
	if (wrdlen > 8) {
		Cycles = ((wrdlen - 1)>>4) + 1; NDhead = wrdlen - (Cycles<<3);

        	for(; Cycles--; str += 8) {
			hash64 = ( hash64 ^ (*(uint64_t *)(str)) ) * PRIME;
			hash64 = ( hash64 ^ (*(uint64_t *)(str+NDhead)) ) * PRIME;
		}
	} else {
		hash64 = ( hash64 ^ _PADr_KAZE(*(uint64_t *)(str+0), (8-wrdlen)<<3) ) * PRIME;
	}
	hash32 = (uint32_t)(hash64 ^ (hash64>>32));
	hash32 ^= (hash32 >> 16);
	return hash32;
} // Last update: 2019-Oct-30, 14 C lines strong, Kaze.


inline static uint32_t simple_hash( void *buf )
{
    return  FNV1A_Pippip_Yurii( buf, HASH_SIZE ) & HASH_TABLE_MASK;
}

/** find or insert variable (buf) in hash-table (hash)
    hash - list of integer
    buf  - pointer to buffer with HASH_SIZE bytes
 */
inline static int hash_lookup( int hash, void *buf,
		 int (*cmpf)(void *ctx, int pos, void *buf),
		 int (*newf)(void *ctx, void *a), void *ctx  )
{
    int p, *d;
    int hash_item;
    uint32_t c = simple_hash(buf);     /* lookup key in hash-table */
    int *hash_item_list = mls(hash, c); /* list of keys with same hash */

    // new entry
    if( *hash_item_list == 0 ) {
	// insert new item-list in hash-table
	*hash_item_list = m_create(1, sizeof(int) );
	goto new_item;
    }

    // entry found
    // check if the same is already inside
    if(cmpf) {
	m_foreach( *hash_item_list, p, d ) {
	    if( cmpf(ctx, *d, buf) ) return *d;
	}
    }

 new_item:
    // create new item in item-list in hash-table
    if( ! newf ) return -1;
    hash_item = newf(ctx,buf);
    m_put(*hash_item_list, &hash_item);
    return hash_item;
}




inline static char cmp64( void *a, void *b )
{
    register uint64_t *u0 = a;
    register uint64_t *u1 = b;
    return (u0[0] == u1[0]) && (u0[1] == u1[1]) && (u0[2] == u1[2]) && (u0[3] == u1[3]);
}

/** key2 points to the keybuffer that was presented to hash_lookup
 *  key1 is the index of one already stored element in the hashmap
 * returns: 1 if keys match, 0 if keys differ
 */
int var5_compare_keys( void *ctx, int key1, void *key2 )
{
    var_t *v1 = *mvar_get(key1);
    var_t *v2 = (var_t*) key2;
    return cmp64(v1,v2);

    //    return
    //	( v1->group == v2->group ) &&
    //	( strncmp(v1->name,v2->name,MAX_VARNAME)==0 );
}

/** create a new variable
 *
 *
 */
int var5_create_var( void *ctx, void *keyptr )
{
    var_t *key = keyptr;
    int typeid = (intptr_t)ctx;
    int p = mvar_create( typeid );
    var_t *v = *mvar_get(p);
    memcpy(v->name, key->name, MAX_VARNAME  );
    v->group=key->group;

    /* a group-id equals variable-id, it's a container to for variables */
    if(key->group) {
	if( mvar_type(v->group) != VAR_VSET )
	    WARN("Var: %d is not a VSET", v->group);
	mvar_put_integer(v->group, p, -1);
    }
    return p;
}



int var5_lookup( int group, char *name, int  typeid )
{
    if(!HASH_TABLE) {
	HASH_TABLE=m_create( HASH_TABLE_SIZE, sizeof(int) );
	m_setlen(HASH_TABLE,HASH_TABLE_SIZE );
    }

    var_t v;
    v.group = group;

    char *src = name;
    char *dest = v.name;
    int  max = sizeof(v.name);
    while( max && *src ) {
	*dest++ = *src++;
	max--;
    }
    while( max-- ) *dest++ = 0;

    return hash_lookup( HASH_TABLE, &v,
			var5_compare_keys,
			typeid >= 0 ? var5_create_var : NULL,
			(void*)(intptr_t)typeid );
}

void var5_delete_hash( var_t *v )
{
    int p, *d;
    uint32_t c = simple_hash(v); /* lookup key in hash-table */
    int *hash_item_list = mls(HASH_TABLE, c); /* list of keys with same hash */
    m_foreach( *hash_item_list, p, d ) {
	var_t *v1 = *mvar_get(*d);
	if( v1->group == v->group && strncmp(v1->name,v->name, MAX_VARNAME)==0 ) {
	    m_del( *hash_item_list, p);
	    return;
	}
    }
}



void dump_hash_statistics(void)
{
    int colls = 0;
    int items = 0;

    int p;
    int *hash_item_list;

    m_foreach( HASH_TABLE,p,hash_item_list) {
	if( *hash_item_list ) {
	    int c = m_len(*hash_item_list );
	    items += c;
	    colls  += c-1;
	}
    }

    printf("Number of Keys: %d\n", items );
    printf("Number of Collisions: %d\n", colls );

}




void mvar_str_init(  mvar_str_t *se  )
{
  memset( se, 0, sizeof *se );
}

void mvar_str_free(  mvar_str_t *se )
{
  m_free_strings( se->splitbuf,0 );
  m_free( se->values );
  m_free( se->indices );
  m_free( se->buf );
  memset( se,0,sizeof *se);
}


void mvar_str_realloc_buffers( mvar_str_t *se )
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


char *unesc_strndup( const char *s, unsigned n)
{
    uint w;
    int ch, n0;
    char *s0,*ret;
    const char *s1;

    ASSERT(ret = malloc(n+1));
    s0 = ret;
    s1=s;
    n0=n;
    if(s==NULL || n0==0) { *s0++=0; goto leave; } /* case 1: zero bytes */

    while( (ch = (*s0++ = *s1++)) ) {
	/* lookahead */
	if( ch == '\\' ) { 	/* escape code found, look at next character */
	    if( *s1 == '\\' ) { /*double escape - skip this char */
		s1++;
	    }
	    else if( *s1 == '$' ) { /* escaped dollar sign, */
		s0[-1] = '$';	/* overwrite allready stored escape */
		s1++;	      /* skip dollar */
	    }
	    /* there is some character we don't know after an escape */
	    /* better do nothing here */
	}
				/* check_if_max_bytes_copied */
	if( --n0 <= 0) {   /* case 2: max bytes copied without zero */
	    *s0++=0; /* append zero and exit */
	    break;
	}
    }

 leave:
    w = s0-ret;
    if( w != n ) { TRACE(1,"realloc %u", w );
	ret = realloc(ret,w);
    }
    return ret;
}




void mvar_str_parse(mvar_str_t *se, const char *frm)
{
  ASSERT( frm && se );

  mvar_str_realloc_buffers(se); // alloc, or clear buffer

  int b=se->splitbuf;
  char *cp,prev; const char *s,*s0;

  prev=0; s = frm; s0=s;

  for(;;) {

    if( *s == 0 || (*s == '$' && prev != '\\') )
      {
        // prefix ?
        if( s0 != s ) {
	    /* unescape string and copy */
	    cp = unesc_strndup(s0,s-s0); // copy without *s
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
	continue;
      }
    prev = *s;
    s++;
  }
};

/** @brief erzeugt aus *s einen string mit escape zeichen
    @param s2 - 0 oder mls liste
    @param s - ein string der umgewandelt werden soll
    @param quotes - falls quotes==1 werden einfache anführungszeichen um die variable gesetzt
    @return gültiger mls string (liste mit breite 1, string ohne Nullbyte)
 */

static int field_escape(int s2, const char *s, int quotes)
{
  // "*s" ist der zu speichernde string
  // um das sql-kommando zu generieren werden sonderzeichen
  // "escaped". dies ist ein gutes beispiel warum die "mls"
  // speicherverwaltung vorteile bietet. der benötigte speicher
  // von mysql_escape_string muss abgeschätzt und reserviert werden.
  // die gleiche funktion in mls ist viel einfacher zu verwenden
  if( quotes ) m_putc( s2, '\'');
  escape_buf( s2, (char*)s );
  if( quotes ) m_putc( s2, '\'');
  return s2;
}

/**
    @return einen gültigen string - immer
 */
const char* mvar_str_expand( mvar_str_t *se, char *prefix, int row )
{
    const char *val;
    int count,var, index;
    int p;
    char **d, *s;
    int quotes = 0;
    int varname = m_create(100,1);
    int vn=0; // number of variables
    m_clear(se->buf); int buf = se->buf;


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
	if( prefix && *prefix ) {
		s_printf(varname,0,"%s.%s", prefix, s+1 );
	} else {
		s_printf(varname,0,"%s", s+1 );
	}
	
	var = mvar_parse(varname,VAR_STRING);
        index = INT(se->indices, vn ); vn++;
	count = mvar_length(var);
        // expand var
	if( index == 1 ) { // erzeuge eine liste von werten
	    val = mvar_get_string(var,0);
	    field_escape(buf, val, quotes);
	    for( index=1; index < count; index++ ) {
		m_putc(buf, ',' );
		val = mvar_get_string(var,index);
		field_escape(buf, val, quotes);
	    }
	}
	else { // index != 1  i.e. not expand all i.e. index != [*]
	    if( index == 0 ) index=row; // falls kein index angegeben wurde, benutze (row)
	    else index -= 2;

	    if( index < count ) {
		val = mvar_get_string(var,index);
		field_escape(buf, val, quotes);
	    }

	}
      } // variable expandiert
  }

  m_putc( buf, 0 );
  m_free(varname);
  return   mls( buf, 0 );
}

/** expandiert den string frm mit den variablen aus vl
 * @return	einen Zeiger auf den expandierten string
 *		dieser string wird auch als variable
 *		unter dem namen se_string in vl gespeichert
 */
const char*   mvar_str_string( char *prefix, const char *frm )
{
    mvar_str_t se;
    int var, varname;

    mvar_str_init( &se );
    mvar_str_parse( &se, frm );
    const char *s = mvar_str_expand( &se, prefix, 0 );

    /* create a new variable and copy the expanded string into it */
    varname = s_printf(0,0,"%s.se_string", prefix );
    var = mvar_parse(varname, VAR_STRING);
    mvar_put_string( var, (char*)s, 0 );

    m_free(varname);
    mvar_str_free(&se);

    /* return copied string */
    return mvar_get_string(var,0);
}
