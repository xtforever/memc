#include "m_tool.h"
#include "mls.h"
#include <stdint.h>

#define HASH_SIZE (4*8)
#define HASH_BITS 14
#define HASH_TABLE_SIZE (1<<HASH_BITS)
#define HASH_TABLE_MASK (HASH_TABLE_SIZE-1)
int HASH_TABLE = 0;

inline static char
cmp64(void *a, void *b)
{
	register uint64_t *u0 = a;
	register uint64_t *u1 = b;
	return (u0[0] == u1[0]) && (u0[1] == u1[1]) && (u0[2] == u1[2])
	       && (u0[3] == u1[3]);
}

// Dedicated to Pippip, the main character in the 'Das Totenschiff'
// roman, actually the B.Traven himself, his real name was Hermann
// Albert Otto Maksymilian Feige.  CAUTION: Add 8 more bytes to the
// buffer being hashed, usually malloc(...+8) - to prevent out of
// boundary reads!  Many thanks go to Yurii 'Hordi' Hordiienko, he
// lessened with 3 instructions the original 'Pippip', thus:
#define _PADr_KAZE(x, n) (((x) << (n)) >> (n))

static uint32_t
FNV1A_Pippip_Yurii(const char *str, size_t wrdlen)
{
	const uint32_t PRIME = 591798841;
	uint32_t hash32;
	uint64_t hash64 = 14695981039346656037UL;
	size_t Cycles, NDhead;
	if (wrdlen > 8) {
		Cycles = ((wrdlen - 1) >> 4) + 1;
		NDhead = wrdlen - (Cycles << 3);

		for (; Cycles--; str += 8) {
			hash64 = (hash64 ^ (*(uint64_t *)(str))) * PRIME;
			hash64
			    = (hash64 ^ (*(uint64_t *)(str + NDhead))) * PRIME;
		}
	} else {
		hash64
		    = (hash64
		       ^ _PADr_KAZE(*(uint64_t *)(str + 0), (8 - wrdlen) << 3))
		      * PRIME;
	}
	hash32 = (uint32_t)(hash64 ^ (hash64 >> 32));
	hash32 ^= (hash32 >> 16);
	return hash32;
} // Last update: 2019-Oct-30, 14 C lines strong, Kaze.

inline static uint32_t
simple_hash(void *buf)
{
	return FNV1A_Pippip_Yurii(buf, HASH_SIZE) & HASH_TABLE_MASK;
}


/** find or insert variable (buf) in hash-table (hash)
    hash - list of integer
    buf  - pointer to buffer with HASH_SIZE bytes
    returns: handle to item 

    hash = [ a1 a2 a3 ... ] 
    a1 = [ item1 item2 item3 ]

 */
inline static int hash_lookup( int hash, void *buf,
                 int (*cmpf)(void *ctx, int hitem, void *buf),
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





int
main()
{
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 1;

	printf("Hashmap Demonstration\n");

	conststr_free();
	m_destruct();
	return 0;
}
