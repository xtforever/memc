#include "m_csv.h"


int
csv_split(int ln)
{
	int str_open = 0;
	int wl = m_create(2, sizeof(int)); /* my word list */
	int w = m_create(10, 1);           /* my word */
	int p = 0;                         /* scan postion  */
	char ch = 0;                       /* current char */
	int len = m_len(ln);               /* length of line */

	while (1) {
		/* store current word and leave if no more characters in buffer
		 */
		/* special case 1: empty line, 2: terminating zero missing THEN:
		 */
		/* RETURN list of mstr with one empty mstr */
		if (p >= len) {
		leave:
			m_putc(w, 0);
			m_puti(wl, w);
			return wl;
		}
		ch = *(char *)mls(ln, p++);
		if (ch == 0)
			goto leave;		/* quoted string found, start from beginning */
		if (ch == '"') {
			str_open = !str_open;
			continue;
		}
		/* store comma only if not in quoted string */
		if (!str_open && ch == ',') {
			m_putc(w, 0);
			m_puti(wl, w);
			w = m_create(10, 1);
			continue;
		}
		/* simple char found, store in word */
		m_putc(w, ch);
	}
}

int
csv_read_stream(FILE *fp)
{
	int db = m_create(100, sizeof(int));
	int ln = m_create(100, 1);
	while (m_fscan(ln, 10, fp) != -1) {
		m_puti(db, csv_split(ln));
		m_clear(ln);
	}
	m_free(ln);
	return db;
}

int
csv_reader(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp)
		ERR("file %s not found", filename);
	int x = csv_read_stream(fp);
	fclose(fp);
	return x;
}

void
csv_freeptr(void *d)
{
	m_free_list(*(int *)d);
}

	
void csv_free(int csv )
{
	m_free_user(csv, csv_freeptr, 0);
}

