
#include "mls.h"


#include <stdio.h>

typedef struct entry {
    int key;
    int value;
} entry_t;

int cmp_ent( const void *a0, const void *b0 )
{
        const  entry_t *a = a0;
        const  entry_t *b = b0;
        return (a->key) - (b->key);
}

void put_entry( int m, int key, int value )
{
    entry_t ent = { .key = key, .value = value };
    m_binsert( m, &ent, cmp_ent, 0 );
}

int  get_entry( int m, int key )
{
    entry_t ent = { .key = key };
    int p = m_bsearch(&ent,m,cmp_ent);
    if( p < 0 ) return -1;
    entry_t *ent2 = mls(m,p);
    return ent2->value;
}

void sparse_1d(void)
{
    // index by number and find key(string)
    int Arr = m_create(100,sizeof(entry_t));
    
    put_entry(Arr,100, 1);
    put_entry(Arr,1000, 2);
    put_entry(Arr,10000, 3);
    
    printf( "[%d] = %d\n", 100, get_entry(Arr,100) );
    printf( "[%d] = %d\n", 1000, get_entry(Arr,1000) );
    printf( "[%d] = %d\n", 10000, get_entry(Arr,10000) );

    m_free(Arr);
}


    

int main( int argc, char **argv )
{
	m_init();
	trace_level=1;
	
	sparse_1d();

	m_destruct();	
}
