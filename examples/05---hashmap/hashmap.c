#ifndef COMPTAG
#define COMPTAG "Unknown Compiler Options"
#endif
const char *Version = "HASHMAP : MEMC DEMO PROJECT / " COMPTAG;

#include "m_csv.h"

#include <stdint.h>
#include <time.h>

#define HASH_SIZE (4 * 8)
#define HASH_BITS 16
#define HASH_TABLE_SIZE (1 << HASH_BITS)
#define HASH_TABLE_MASK (HASH_TABLE_SIZE - 1)
int HASH_TABLE = 0;
static int collision_count = 0;

inline static char
cmp64(void *a, void *b)
{
	register uint64_t *u0 = a;
	register uint64_t *u1 = b;
	return (u0[0] == u1[0]) && (u0[1] == u1[1]) && (u0[2] == u1[2])
	       && (u0[3] == u1[3]);
}

// I leave the original comment here:
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

struct hash_item {
	int key, val;
};

/* returns:
   if val < 0:
      return val if key is found, otherwise return -1
   if val >= 0:
       return val if key is existing in hashmap,
       return -1  if key was inserted in hashmap
*/

inline static int
hash_lookup(int hash, int key, int val)
{
	if (m_len(key) < HASH_SIZE) {
		m_setlen(key, HASH_SIZE); /* resize key if it is too short */
	}
	uint32_t c = simple_hash(m_buf(key)); /* lookup key in hash-table */
	int *hash_item_list = mls(hash, c);   /* list of keys with same hash */
	int p;
	struct hash_item data = { .key = key, .val = val };

	if (val < 0) { /* lookup; do not insert anything */
		if (*hash_item_list == 0)
			return -1;
		p = m_bsearch(&data, *hash_item_list, cmp_mstr_fast);
		if (p < 0)
			return -1;
		return ((struct hash_item *)mls(*hash_item_list, p))->val;
	}

	/* lookup and insert if not exists */
	if (*hash_item_list == 0) { /* no hashmap: create one and insert key */
		*hash_item_list = m_create(1, sizeof(data));
		m_put(*hash_item_list, &data);
		return val;
	}

	/* one or more items in list: use binary search */
	p = m_binsert(*hash_item_list, &data, cmp_mstr_fast, 0);

	if (p < 0) { /* item found in hashmap */
		return ((struct hash_item *)mls(*hash_item_list, -p - 1))->val;
	}
	/* item was inserted in hashmap */
	collision_count++;
	return -1;
}


#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"


void
print_csv_data(int wds, int data)
{
	int p, *d;
	m_foreach(data, p, d)
	{
		if (p >= m_len(wds))
			break;
		int w = INT(wds, p);
		if (p)
			printf("| ");
		printf("%*.*M", w, w, *d);
	}
	printf("\n");
}

/**
 * @fn timespec_diff(struct timespec *, struct timespec *, struct timespec *)
 * @brief Compute the diff of two timespecs, that is a - b = result.
 * @param a the minuend
 * @param b the subtrahend
 * @param result a - b
 */
static inline void
timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result)
{
	result->tv_sec = a->tv_sec - b->tv_sec;
	result->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (result->tv_nsec < 0) {
		--result->tv_sec;
		result->tv_nsec += 1000000000L;
	}
}

static struct timespec tp0;
void
timer_start(void)
{
	clock_gettime(CLOCK_MONOTONIC, &tp0);
}

double
timer_stop(void)
{
	struct timespec tp1, tp2;
	clock_gettime(CLOCK_MONOTONIC, &tp1);
	timespec_diff(&tp1, &tp0, &tp2);
	long double n = tp2.tv_sec;
	n = n * 1E6 + (tp2.tv_nsec / 1E3);
	return n;
}

void
demonstrate_csv_reader(void)
{
	const char *csv_file = "demo.csv";
	int csv = csv_reader(csv_file);
	if (m_len(csv) < 2) {
		ERR("Not enough data in csv file %s", csv_file);
	}

	int p, *d;

	/* calculate widths of columns, array wds must have at least 'column'
	 * entries */
	int wds = m_create(10, sizeof(int));
	int data = INT(
	    csv,
	    1); /* first(0) line contains column names, (1..n) contains data */
	int header = INT(csv, 0); /* first line contains column names */
	int columns = m_len(header);
	m_setlen(wds, columns); /* resize array to width 'columns' */
	/* put max size of each data element in array wds */
	m_foreach(csv, p, d)
	{
		int p1, *d1;
		m_foreach(*d, p1, d1)
		{
			INT(wds, p1) = Max(m_len(*d1) + 2, INT(wds, p1));
		}
	}
	/* fill array wds with '2' up to columns */
	for (p = m_len(data); p < columns; p++) {
		m_puti(wds, 2);
	}
	/* shorten array if it has too many entries */
	m_setlen(wds, columns);
	/* create entries consiting of dashes */
	int hdash = m_create(columns, sizeof(int));
	for (int i = 0; i < columns; i++) {
		int w = INT(wds, i);
		/* create byte array, filled with dash, append to list hdash */
		m_puti(hdash, m_memset(0, '-', w));
	}

	/* print header */
	print_csv_data(wds, header);
	print_csv_data(wds, hdash);

	/* print some lines of data */
	for (int i = 1; i < 3; i++) {
		print_csv_data(wds, INT(csv, i));
	}

	/* insert email into hashmap
	   find Email column
	*/
	int hash = m_create(HASH_TABLE_SIZE, sizeof(int));
	m_setlen(hash, HASH_TABLE_SIZE);

	/* search the array of handles for the string 'Emails' */
	char *em = "Email";
	int email_field = m_lfind(&em, header, cmp_mstr_cstr_fast); /*!X! */
	if (p < 0) {
		WARN("No Email column in csv");
		goto leave;
	}

	timer_start();
	m_foreach(csv, p, d)
	{
		if (email_field >= m_len(*d))
			continue; /* no email column in this line */
		int email = INT(*d, email_field);
		hash_lookup(hash, email, p); /* store line number and email */
	}
	printf("Created hashmap for %d entries. %f  hash/µsec, collison: %d, "
	       "size: %d\n",
	       m_len(csv), m_len(csv) / timer_stop(), collision_count,
	       HASH_TABLE_SIZE);

	long long int count = 100000000;
	printf("Executing %lld hashes\n", count);
	timer_start();
	for (int i = 0; i < count; i++) {
		/* random number module number of lines in this csv */
		int keynum = rand() % m_len(csv);
		/* fetch handle to line 'keynum' */
		int ln = INT(csv, keynum);
		/* check if this line has enough fields */
		if (m_len(ln) < email_field) {
			continue;
		}
		/* entry at positon 'email_field' from line 'ln'  */
		int email = INT(ln, email_field);
		/* find line numer in hashmap by email  */
		int x = hash_lookup(hash, email, -1);
		if (x != keynum) {
			WARN("email %M not found", email);
		}
	}
	double h = count / timer_stop();
	printf("hashmap speed: %f hash/µsec\n", h);

leave:
	m_free_list(hash);
	csv_free(csv);
	m_free(wds);
	m_free_list(hdash);
}

void
csv_split1(int m)
{
	int p, *d;
	int ln = csv_split(m);
	m_foreach(ln, p, d) { printf("%c%d:%M", p?',':' ',p, *d); }
	putchar(10);
	m_free_list(ln);
}

void
demonstrate_csv_split(void)
{

	csv_split1(s_cstr("hello,world"));
	csv_split1(s_cstr("hello"));
	csv_split1(s_cstr("hell,\"o,w\",orld"));
	csv_split1(s_cstr("hell,\"o,w\""));
	csv_split1(s_cstr("hell,\"\""));
	csv_split1(s_cstr("\"\"hell,\"\""));
	csv_split1(s_cstr(""));
	csv_split1(s_cstr(","));
	csv_split1(s_cstr("\"\","));
	csv_split1(s_cstr("\""));
}

int
main()
{
	printf("Version: %s\n", Version);
	m_init();
	conststr_init();
	m_register_printf();
	trace_level = 0;

	// demonstrate_csv_split();

	printf("Hashmap Demonstration\n");
	demonstrate_csv_reader();

	conststr_free();
	m_destruct();
	return 0;
}
