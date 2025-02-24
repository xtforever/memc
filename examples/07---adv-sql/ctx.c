#include "ctx.h"
#include "mls.h"

/* alloc structure, re-use alloced but unsused structs 
   first element in struct must be
   int init_flag;
   returns >=0: index to struct 
*/ 
int ctx_init(int *CTX, int length, int size)
{
    if(! *CTX) {
	 *CTX = m_create( length, size);
    }
    
    /* find empty element */
    int *d;
    int p;
    m_foreach(*CTX,p,d) {
	if( *d == 0 ) goto found;
    }
    p = m_new(*CTX,1);
    d = mls(*CTX, p);
 found:
    *d=1;
    return p;
}

void ctx_free(int *CTX, int p, void (*free_cb)(int *ctx, int p))
{
    if( free_cb ) free_cb(CTX,p);         /* user data free */
    int *d = mls(*CTX,p);   
    *d=0;                   /* internal data free */

    /* clear CTX if no struct is left */
    m_foreach( *CTX, p, d ) {
	if( *d ) return;
    }
    m_free(*CTX);
    *CTX=0;
}

void ctx_destruct(int *CTX, void (*free_cb)(int *ctx, int p))
{
    int p;
    int *d;
    if( free_cb ) {
	m_foreach( *CTX, p, d ) {
	    if( *d ) free_cb(CTX, p);
	}
    }
    m_free(*CTX);
    *CTX=0;
}

