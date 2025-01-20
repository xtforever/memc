#include "mls.h"
#include <stdio.h>

typedef struct entry {
	int key;
	int value;
} entry_t;

int
cmp_ent(const void *a0, const void *b0)
{
	const entry_t *a = a0;
	const entry_t *b = b0;
	return (a->key) - (b->key);
}

void
put_entry1(int m, int key, int value)
{
	entry_t ent = { .key = key, .value = value };
	int pos2 = m_binsert(m, &ent, cmp_ent, 0);
	if (pos2 < 0) /* already exists, overwrite */
	{
		pos2 = -pos2 - 1;
		((entry_t *)mls(m, pos2))->value = value;
	}
}

int
get_entry1(int m, int key)
{
	entry_t ent = { .key = key };
	int p = m_bsearch(&ent, m, cmp_ent);
	if (p < 0)
		return -1;
	entry_t *ent2 = mls(m, p);
	return ent2->value;
}

void
sparse_1d(void)
{
	// index by number and find key(string)
	int Arr = m_create(100, sizeof(entry_t));

	put_entry1(Arr, 100, 1);
	put_entry1(Arr, 1000, 2);
	put_entry1(Arr, 10000, 3);

	printf("[%d] = %d\n", 100, get_entry1(Arr, 100));
	printf("[%d] = %d\n", 1000, get_entry1(Arr, 1000));
	printf("[%d] = %d\n", 10000, get_entry1(Arr, 10000));

	m_free(Arr);
}

int
create_or_get_entry(int m, int x)
{
	int row = m_create(1, sizeof(entry_t));
	entry_t ent = { .key = x, .value = row };
	int pos = m_binsert(m, &ent, cmp_ent, 0);
	if (pos >= 0)
		return row;
	/* already exists */
	pos = -pos - 1;
	m_free(row);
	return ((entry_t *)mls(m, pos))->value;
}

void
put_entry2(int m, int x, int key, int value)
{
	int row;
	row = create_or_get_entry(m, x); /* row = m[x] */
	put_entry1(row, key, value);     /* row[key] */
}

int
get_entry2(int m, int x, int key)
{
	int handle;
	handle = get_entry1(m, x);
	if (handle > 0) {
		return get_entry1(handle, key);
	}
	return -1; /* not found */
}

void
sparse_2d(void)
{
	// index by number and find key(string)
	int Arr = m_create(100, sizeof(entry_t));

	put_entry2(Arr, 5, 100, 1);
	put_entry2(Arr, 1000, 1000, 2);
	put_entry2(Arr, 999, 10000, 3);

	printf("[%d,%d] = %d\n", 5, 100, get_entry2(Arr, 5, 100));
	printf("[%d,%d] = %d\n", 1000, 1000, get_entry2(Arr, 1000, 1000));
	printf("[%d,%d] = %d\n", 999, 10000, get_entry2(Arr, 999, 10000));

	int p;
	entry_t *d;
	m_foreach(Arr, p, d) m_free(d->value);
	m_free(Arr);
}

int
main(int argc, char **argv)
{
	m_init();
	trace_level = 1;

	sparse_1d();
	sparse_2d();

	m_destruct();
}
