#include "m_tool.h"
#include "mls.h"
#include <stdarg.h>
#include <printf.h>

/* alphabetisch sortierte liste von mstr */
static int CONSTSTR_DATA = 0;
static int CS_ZERO = -1;

// Custom printf handler function for the 'M' specifier
static int
mls_printf_handler(FILE *stream, const struct printf_info *info,
                   const void *const *args)
{
	// Extract the argument as a "mls" type
	const int p = *((const int *)args[0]);
	// Convert mls Handle to String
	const char *s = mls(p, 0);
	char *o, out[50];
	o = out;
	*o++ = '%';
	if (info->left) {
		*o++ = '-';
	}
	if (info->width > 0) {
		o += sprintf(o, "%u", info->width);
	}
	if (info->prec > 0) {
		o += sprintf(o, ".%u", info->prec);
	}
	*o++ = 's';
	*o++ = 0;

	// Print the mls string to the stream using given modifier
	return fprintf(stream, out, s);
}
// Custom argument size handler for the 'M' specifier
static int
mls_printf_arginfo(const struct printf_info *info, size_t n, int *argtypes,
                   int *size)
{
	if (n > 0) {
		argtypes[0] = PA_INT; // Expecting 'int'
	}
	return 1;
}

void
m_register_printf()
{
	// Register the custom printf specifier 'M'
	if (register_printf_specifier('M', mls_printf_handler,
	                              mls_printf_arginfo)
	    != 0) {
		ERR("Failed to register printf specifier 'M'\n");
	}
}

int
m_str_va_app(int m, va_list ap)
{
	char *name;
	while ((name = va_arg(ap, char *)) != NULL) {
		name = strdup(name);
		m_put(m, &name);
	}
	return m;
}

int
m_str_app(int m, ...)
{
	va_list ap;
	if (!m)
		m = m_create(3, sizeof(char *));
	va_start(ap, m);
	m_str_va_app(m, ap);
	va_end(ap);
	return m;
}

static void
cskip_ws(char **s)
{
	while (isspace(**s))
		(*s)++;
}

static void
ctrim(char **a, char **b)
{
	while (*a < *b && isspace((*b)[-1])) {
		(*b)--;
	}
}
static void
skip_delim(char **s, char *delim)
{
	while (**s && *delim && **s == *delim) {
		(*s)++;
		delim++;
	}
}

static void
skip_unitl_delim(char **s, char *delim)
{
	char *p = strstr(*s, delim);

	if (p) { /* delim found, set s to start of delim */
		*s = p;
		return;
	}

	while (**s)
		(*s)++; /* delim not found, set s to end of string */
}

static int
cut_word(char **s, char *delim, int trimws, char **a, char **b)
{

	// remove ws from start word
	if (trimws)
		cskip_ws(s);
	*a = *s;                    // start of word
	skip_unitl_delim(s, delim); // s points to first char of delimiter
	*b = *s;                    // b points to first char after word
	if (trimws)
		ctrim(a, b); // start at b and remove ws until a
	return (*b) - (*a);
}

static char *
dup_word(char **s, char *delim, int trimws)
{
	char *a, *b;
	int len = cut_word(s, delim, trimws, &a, &b);
	if (len) {
		char *word = calloc(1, len + 1);
		memcpy(word, a, b - a);
		word[len] = 0;
		return word;
	}
	return NULL;
}

/** split string into list of c-strings
 **/
int
m_str_split(int m, char *s, char *delim, int trimws)
{
	if (!m)
		m = m_create(3, sizeof(char *));
	char *w;

	while (*s) {
		w = dup_word(&s, delim, trimws);
		if (w)
			m_put(m, &w);
		skip_delim(&s, delim);
	}

	return m;
}

/** @brief copy string from src to dest
 * the dst buffer is always resized to max and will contain a termination zero
 * an empty or not-allocated (i.e. zero) src is allowed
 * if dst is zero it will be allocated
 * @returns dst
 */
int
m_strncpy(int dst, int src, int max)
{
	int c, src_bytes;
	ASSERT(max > 0);

	/* create string if none given */
	if (!dst)
		dst = m_create(max, 1);
	else if (m_bufsize(dst) < max) {
		/* realloc if string is too short */
		m_setlen(dst, max);
	}

	/* if src not exists or has zero length, return empty string */
	if (src < 1 || !(src_bytes = m_len(src))) {
		m_putc(dst, 0);
		return dst;
	}

	/* copy src to dst, but not more than max-bytes */
	c = Min(max, src_bytes);
	memcpy(m_buf(dst), m_buf(src), c);
	m_setlen(dst, c);

	/* if the string is not zero terminated, append zero */
	if (CHAR(dst, c - 1) != 0) {
		if (c < max)
			m_putc(dst, 0);
		else
			CHAR(dst, c - 1) = 0;
	}

	return dst;
}

static void
array_copy(int dest, int destp, int src, int srcp, int src_count)
{
	void *from = mls(src, srcp);
	void *to = mls(dest, destp);
	size_t n = src_count * m_width(src);
	memcpy(to, from, n);
}

static void
element_copy(int dest, int destp, int src, int srcp, int src_count, int width)
{
	for (int i = 0; i < src_count; i++) {
		void *from = mls(src, srcp);
		memcpy(mls(dest, destp), from, width);
		srcp++;
		destp++;
	}
}

/** @brief copy *src_count* elements from *src* to *dest*
 * if *dest* is <=0 create a new array
 * if *src_count* is -1 or too big it is set to copy all of *src* starting at
 * *srcp* if *destp* is <0 append *src* to *dest*
 * @returns dest
 */
int
m_mcopy(int dest, int destp, int src, int srcp, int src_count)
{
	int dest_len;
	int src_len = m_len(src);

	if (srcp < 0)
		srcp = 0;
	/* if no lenght is given or length outside of array, copy full array */

	if (src_count < 0 || src_count + srcp > m_len(src)) {
		src_count = src_len - srcp;
	}

	/* so far, we have checked srcp,src_count, now look at the others */

	/* check number of elements in destination array: dest_len,
	   create dest array if it doesn't exists
	   if no dest offset is given assume user wants to append
	*/
	if (dest <= 0) {
		if (destp > 0)
			dest_len = destp + src_count;
		else
			dest_len = src_count;
		dest = m_create(dest_len, m_width(src));
	} else {

		if (destp < 0) {
			destp = m_len(
			    dest); /* this means: append to src to dest array */
		}
		dest_len = destp + src_count;
	}

	/* resize dest to fit src array only if it is currently too small */
	if (m_len(dest) < dest_len)
		m_setlen(dest, destp + src_count);

	/* shortcurt, nothing to copy */
	if (src_count == 0)
		return dest;

	/* now we have checked: dest_len,dest,src_count,srcp,destp */

	/* check if src and dest are of same element size */
	if (m_width(src) == m_width(dest)) {
		array_copy(dest, destp, src, srcp, src_count);
		return dest;
	}

	/* copy each element from src to dest */
	int width = Min(m_width(src), m_width(dest));
	element_copy(dest, destp, src, srcp, src_count, width);
	return dest;
}

int
compare_int(const void *a, const void *b)
{
	const int *a0 = a, *b0 = b;
	return (*a0) - (*b0);
}

void
m_free_ptr(void *d)
{
	m_free(*(int *)d);
}

void
m_free_user(int m, void (*free_h)(void *), int only_clear)
{
	void *d;
	int p;
	m_foreach(m, p, d) free_h(d);
	if (only_clear)
		m_clear(m);
	else
		m_free(m);
}

void
m_clear_user(int m, void (*free_h)(void *))
{
	m_free_user(m, free_h, 1);
}

/* free a list of lists */
void
m_free_list(int m)
{
	if( m_width(m) < sizeof(int) ) {
		ERR("expected array of handles/int but list %d has width %d",
		    m, m_width(m) );
	}
	
	m_free_user(m, m_free_ptr, 0);
}

/* clear a list of lists */
void
m_clear_list(int m)
{
	m_free_user(m, m_free_ptr, 1);
}

/* free a list of (malloced char*) */
void
m_free_stringlist(int m)
{
	m_free_user(m, free, 0);
}

/* clear a list of (malloced char*) */
void
m_clear_stringlist(int m)
{
	m_free_user(m, free, 1);
}

void
m_concat(int a, int b)
{
	if (!a || !b)
		return;
	int p;
	void *d;
	m_foreach(b, p, d) m_put(a, d);
}

/** split a string by one character *delm into multiple m-array strings
 * @returns list of m-arrays
 **/
int
m_split_list(const char *s, const char *delm)
{
	int ls = m_create(2, sizeof(int));
	int w = 0;
	int p = 0;
	int ch;

	do {
		ch = s[p];
		if (!w) {
			w = m_create(10, 1);
			m_put(ls, &w);
		}

		if (ch == *delm || ch == 0) {
			m_putc(w, 0);
			w = 0;
		} else {
			m_putc(w, s[p]);
		}
		p++;
	} while (ch);

	return ls;
}

/* copy from s to buf[p] until *s == ch */
int
leftstr(int buf, int p, const char *s, int ch)
{
	int w;
	if (buf < 2)
		buf = m_create(10, 1);
	m_setlen(buf, p);

	if (s)
		while ((w = *s++)) {
			if (w == ch || w == 0)
				break;
			m_putc(buf, w);
		}
	m_putc(buf, 0);
	return buf;
}


/* einfuegen von key in das array m falls key noch nicht
   existiert
   return: pos von key
*/
int
lookup_int(int m, int key)
{
	void *obj = calloc(1, m_width(m));
	memcpy(obj, &key, sizeof(key));
	int p = m_binsert(m, obj, cmp_int, 0);
	free(obj);
	if (p < 0) {
		return (-p) - 1;
	}
	TRACE(1, "ADD pos:%d key:%d", p, key);
	return p;
}

/**
 * @brief Copies a portion of the list `m` starting at index `a` and ending at
 * index `b` to a new list at position `offs`, and returns the new list.
 *
 * - If `dest` is zero, a new destination list is created.
 * - Indices can be positive or negative:
 *   - Negative indices count from the end to the start of the list.
 *   - The first element is 0, and the last element is -1.
 *
 * Example:
 * @code
 *   m:    0    1    2    3    4
 *       -5   -4   -3   -2   -1
 * @endcode
 *
 * @param dest The destination list where the portion is copied. If set to 0, a
 * new list is created.
 * @param offs The offset position in the destination list where the copied
 * portion is placed.
 * @param m The source list from which the portion is copied.
 * @param a The starting index of the portion to be copied.
 * @param b The ending index of the portion to be copied.
 * @return The new list with the copied portion.
 */
int
m_slice(int dest, int offs, int m, int a, int b)
{
	int len = m ? m_len(m) : 0;
	if (b < 0) {
		b += len;
	}
	if (a < 0) {
		a += len;
	}
	if (b >= len)
		b = len - 1;
	if (a >= len)
		a = len - 1;
	if (a < 0)
		a = 0;
	int cnt = b - a + 1;
	if (cnt < 0)
		cnt = 1;
	if (dest <= 0)
		dest = m_create(cnt + offs, m_width(m));
	m_setlen(dest, offs);
	ASSERT(m_width(dest) == m_width(m));
	for (int i = a; i <= b; i++) {
		void *d = m_peek(m, i);
		m_put(dest, d);
	}
	return dest;
}

/* slice for strings: add a zero byte to the end */
int
s_slice(int dest, int offs, int m, int a, int b)
{
	int ret = m_slice(dest, offs, m, a, b);
	if (m_len(ret) == 0 || CHAR(ret, m_len(ret) - 1) != 0)
		m_putc(ret, 0);
	return ret;
}

int
s_warn(int m)
{
	if (m) {
		if (m_width(m) != 1)
			WARN("mstring %d to width: %d bytes", m, m_width(m));
		if (m_len(m) && CHAR(m, m_len(m) - 1) != 0)
			WARN("mstring %d not zero terminated", m);
	}
	return 0;
}

/* verify that m is a not zero-length zero terminated string */
int
s_isempty(int m)
{
	if (m == 0)
		WARN("handle is zero");
	else {
		if (m_len(m) == 0)
			WARN("strlen  is zero");
		else {
			if (CHAR(m, m_len(m) - 1))
				WARN("last byte not zero");
			if (CHAR(m, 0) == 0)
				WARN("first byte is zero");
		}
	}

	return (m == 0 || m_len(m) == 0 || CHAR(m, 0) == 0 || m_width(m) != 1
	        || CHAR(m, m_len(m) - 1) != 0);
}

void
s_write(int m, int n)
{
	if (s_isempty(m))
		return;
	char *d;
	int p;
	m_foreach(m, p, d)
	{
		if (n <= 0 || *d == 0)
			return;
		putchar(*d);
		n--;
	}
}

void
s_puts(int m)
{
	s_write(m, m_len(m));
	putchar(10);
}

/**
 * Compares the 'pattern' with the m-array 'm' starting at the offset 'offs'.
 * <p>
 * If 'n' is zero, the comparison could include the trailing zero in 'pattern'
 * if 'pattern' is in C-string style. If 'n' is non-zero, up to 'n' characters
 * will be compared. This function returns 0 if 'pattern' matches 'm' or the
 * difference between the values of the last two compared characters.
 * </p>
 *
 * @param m the m-array to be compared against
 * @param offs the offset in 'm' where comparison begins
 * @param pattern the pattern to compare with
 * @param n the maximum number of characters to compare; if zero, may include
 * trailing null
 * @return 0 if 'pattern' matches 'm', or the difference between the values of
 * the last two compared characters if they don't match
 */
int
s_strncmp(int m, int offs, int pattern, int n)
{
	/* empty pattern matches every char */
	if (s_isempty(pattern) || n <= 0)
		return 0;
	/* empty source does not match */
	if (s_isempty(m))
		return -1;

	int p;
	char *d;
	int diff;
	m_foreach(pattern, p, d)
	{
		if (offs + p >= m_len(m))
			return -1;
		diff = CHAR(m, offs + p) - *d;
		if (diff)
			return diff;
		n--; /* when <0 after first match, ignore n */
		if (n == 0)
			return 0;
	}
	return 0;
}

int
s_strstr(int m, int offs, int pattern)
{
	if (s_isempty(m) || s_isempty(pattern) || offs < 0)
		return -1;
	int plen = m_len(pattern);
	int max = m_len(m) - plen;
	for (; offs <= max; offs++) {
		if (s_strncmp(m, offs, pattern, plen - 1) == 0)
			return offs;
	}
	return -1;
}

/**
 * Replaces occurrences of a specified pattern in the source string with a given
 * replacement string. This function processes the source string and performs
 * the replacement up to a given number of times.
 *
 * @param dest The destination string that will store the result after
 * replacements. It is expected to be initialized to a valid string or set to
 * zero
 * @param src The source string in which the replacements will be performed.
 * This string is the one being searched for the specified pattern.
 * @param pattern The string value representing the pattern to be searched for
 * in the source integer.
 * @param replace The string value that will replace the found pattern
 * occurrences.
 * @param count The maximum number of replacements to perform. If 0, all
 * occurrences are replaced.
 *
 * @return The modified destination string with the replacements applied.
 *         The function will return the destination string after replacing up to
 * the specified count of pattern occurrences with the replacement value.
 */
int
s_replace(int dest, int src, int pattern, int replace, int count)
{
	if (dest == 0)
		dest = m_create(20, 1);
	else
		m_clear(dest);
	ASSERT(m_width(dest) == 1);
	if (s_isempty(src) || s_isempty(pattern))
		return dest;
	int offs = 0;
	int replace_last = s_strlen(replace) - 1;
	int pattern_len = s_strlen(pattern);
	for (;;) {
		int pos = s_strstr(src, offs, pattern);
		if (pos < 0)
			break;
		m_slice(dest, m_len(dest), src, offs, pos - 1);
		m_slice(dest, m_len(dest), replace, 0, replace_last);
		offs = pos + pattern_len;
		if (--count == 0)
			break;
	}
	m_slice(dest, m_len(dest), src, offs, -1);
	return dest;
}

int
s_msplit(int dest, int src, int pattern)
{
	if (dest == 0)
		dest = m_create(10, sizeof(int));
	else
		m_clear(dest);
	if (s_isempty(src) || s_isempty(pattern))
		return dest;
	int offs = 0;
	int pattern_len = s_strlen(pattern);
	int substr;
	for (;;) {
		int pos = s_strstr(src, offs, pattern);
		if (pos < 0)
			break;
		substr = s_slice(0, 0, src, offs, pos - 1);
		m_puti(dest, substr);
		offs = pos + pattern_len;
	}
	if (offs < m_len(src) - 1) {
		m_puti(dest, m_slice(0, 0, src, offs, -1));
	}

	return dest;
}

int
s_trim(int m)
{
	if (m == 0)
		return 0;
	int a = 0;
	int b = m_len(m) - 1;
	if (b < 0) { /* make c-str */
		m_putc(m, 0);
		return m;
	}
	if (CHAR(m, b) == 0)
		b--;
	while (a < b && isspace(CHAR(m, a)))
		a++;
	while (b >= a && isspace(CHAR(m, b)))
		b--;
	if (a == 0) {
		m_setlen(m, b + 1);
		m_putc(m, 0);
		return m;
	}
	return s_slice(m, 0, m, a, b);
}

int
s_lower(int m)
{
	if (s_isempty(m))
		return m;
	int p;
	char *d;
	m_foreach(m, p, d) *d = tolower(*d);
	return m;
}

int
s_upper(int m)
{
	if (s_isempty(m))
		return m;
	int p;
	char *d;
	m_foreach(m, p, d) *d = toupper(*d);
	return m;
}

void
m_map(int m, int (*fn)(int m, int p, void *ctx), void *ctx)
{
	if (m == 0 || fn == 0)
		return;
	int n = m_len(m);
	for (int i = 0; i < n; i++)
		fn(m, i, ctx);
}


int
s_strcpy_c(int out, const char *s)
{
	
	if( out <= 0 ) out = m_create(1,1);
	if( !s || !*s ) { m_putc(out,0); return out; }
	m_write(out,0,s,strlen(s)+1);
	return out;
}

int
s_strcmp_c(int s0,int offs, const char *s1)
{
	if( s0 == 0 || m_len(s0) <= offs ) return -1;
	return mstrcmp(s0,offs,s1);	
}


int
s_strdup_c(const char *s)
{
	if (s == NULL) {
		int v = m_create(1, 1);
		m_putc(v, 0);
		return v;
	}

	int len = strlen(s) + 1;
	int v = m_create(len, 1);
	m_write(v, 0, s, len);
	return v;
}

int
s_implode(int dest, int srcs, int seperator)
{
	if (dest == 0)
		dest = m_create(10, 1);
	else
		m_clear(dest);
	if (srcs <= 0 || m_len(srcs) == 0 || s_isempty(seperator))
		goto leave;

	int p, *d;
	m_foreach(srcs, p, d)
	{
		if (p) {
			m_slice(dest, m_len(dest), seperator, 0, -2);
		}
		m_slice(dest, m_len(dest), *d, 0, -2);
	}
leave:
	m_putc(dest, 0);
	return dest;
}

int m_memset(int ln, char c, int w)
{
	if( ln  == 0 ) ln = m_create(w,1);
	m_setlen(ln,w);
	if( m_width(ln) == 1 )
		memset(m_buf(ln),c,w);
	else {
		for(int i=0;i<w;i++) {
			*(char *)mls(ln,i) = c;
		}		
	}
	return ln;
}



void
conststr_free(void)
{
	m_free_list(CONSTSTR_DATA);
	CONSTSTR_DATA = 0;
}

void
conststr_init(void)
{
	if (!CONSTSTR_DATA) {
		CONSTSTR_DATA = m_create(10, sizeof(int));
		CS_ZERO = m_create(1, 1);
		m_putc(CS_ZERO, 0);
		m_puti(CONSTSTR_DATA, CS_ZERO);
	}
}

int
cmp_mstr(const void *a, const void *b)
{
	int k1 = *(const int *)a;
	int k2 = *(const int *)b;
	TRACE(1, "cmp %s %s", CHARP(k1), CHARP(k2));
	return m_cmp(k1, k2);
}

/* treat shorter strings as beeing lower than longer strings */
int
cmp_mstr_fast(const void *a, const void *b)
{
	int k1 = *(const int *)a;
	int k2 = *(const int *)b;
	TRACE(1, "cmp %s %s", CHARP(k1), CHARP(k2));
	if (m_len(k1) < m_len(k2))
		return -1;
	if (m_len(k1) > m_len(k2))
		return 1;
	return m_cmp(k1, k2);
}

int cmp_mstr_cstr_fast(const void *key, const void *b)
{
	const char *const *s = key;
	int mstr = *(int*) b;
	return -mstrcmp(mstr, 0, *s);	
}

static int
mscmpc(const void *a, const void *b)
{
	const char *const *s = a;
	const int *d = b;
	return -mstrcmp(*d, 0, *s);
}

int m_str_from_file( char *filename )
{
	FILE *fp = fopen( filename, "r" );
	int db = m_create(100, sizeof(int));
	if(!fp) return db;
	int ln;
	while(1) {
		ln = m_create(100, 1);
		if( m_fscan(ln, 10, fp) < 0 ) break;
		m_puti(db, ln );
	}
	m_free(ln);	
	fclose(fp);	
	return db;     
}


/* schau nach ob s schon vorhanden ist, wenn ja liefere die vorhandene kopie,
   sonst fuege ein kopie von s hinzu IDEE: x=m_create(); m_put(x,data); z=
   conststr_lookup(x); free(x);
 */
int
conststr_lookup(int s)
{
	if (s_empty(s))
		return CS_ZERO;
	int p = m_binsert(CONSTSTR_DATA, &s, cmp_mstr_fast, 0);
	if (p < 0) { // schon vorhanden
		return INT(CONSTSTR_DATA, (-p) - 1);
	}
	TRACE(1, "ADD %d %s", p, CHARP(s));
	return INT(CONSTSTR_DATA, p) = m_dub(s);
}

int
conststr_lookup_c(const char *s)
{
	if (is_empty(s))
		return CS_ZERO;
	union {
		const char *s;
		int key;
	} key;
	key.s = s;
	int p = m_binsert(CONSTSTR_DATA, &key, mscmpc, 0);
	if (p < 0) {
		return INT(CONSTSTR_DATA, (-p) - 1);
	}
	int k = s_printf(0, 0, "%s", s);
	INT(CONSTSTR_DATA, p) = k;
	return k;
}

/**
 * @brief Creates a constant string using vas_printf that does not need
 * deallocation. Uses vas_printf to generate a temporary string. Searches for
 * this string in the global storage:
 *        - If found, returns a handle to the existing string and frees the
 * temporary string.
 *        - Otherwise, inserts the temporary string into the global storage and
 * returns it.
 */
int
cs_printf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int m = vas_printf(0, 0, format, ap);
	va_end(ap);
	int p = m_binsert(CONSTSTR_DATA, &m, cmp_mstr, 0);
	if (p < 0) { // schon vorhanden
		m_free(m);
		return INT(CONSTSTR_DATA, (-p) - 1);
	}
	TRACE(1, "ADD const[%d]=%d:%s", p, m, CHARP(m));
	return m;
}

void
conststr_stats(void)
{
	int len, *d, p;
	int cnt = m_len(CONSTSTR_DATA);
	TRACE(4, "Count: %d", cnt);
	len = cnt * (sizeof(int) + sizeof(struct ls_st));
	m_foreach(CONSTSTR_DATA, p, d) { len += m_len(*d); }
	TRACE(4, "Memory: %d", len);
}

int
s_strncmp2(int s0, int p0, int s1, int p1, int len)
{
	int l0 = m_len(s0);
	int l1 = m_len(s1);

	while (len--) {
		if (p0 >= l0)
			return -1;
		if (p1 >= l1)
			return 1;
		int diff = CHAR(s0, p0) - CHAR(s1, p1);
		if (diff || CHAR(s0, p0) == 0)
			return diff;
		p0++;
		p1++;
	}
	return 0;
}

int
s_strncmpr(int str, int suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = m_len(str);
	size_t lensuffix = m_len(suffix);
	if (lensuffix > lenstr)
		return 0;
	return s_strncmp2(str, lenstr - lensuffix, suffix, 0, lensuffix) == 0;
}

int
s_readln(int buf, FILE *fp)
{
	m_clear(buf);
	int ret = m_fscan(buf, 10, fp);
	if (ret < 0 && m_len(buf) > 1)
		return 0;
	return ret;
}

int
s_regex(int res, char *regex, int buf)
{
	m_regex(res, regex, m_str(buf));
	return m_len(res);
}

/* LICENSE Dual MIT/GPL
   ripped from linux kernel 6.8 source /lib/glob.c
*/

/**
 * glob_match - Shell-style pattern matching, like !fnmatch(pat, str, 0)
 * @pat: Shell-style pattern to match, e.g. "*.[ch]".
 * @str: String to match.  The pattern must match the entire string.
 *
 * Perform shell-style glob matching, returning true (1) if the match
 * succeeds, or false (0) if it fails.  Equivalent to !fnmatch(@pat, @str, 0).
 *
 * Pattern metacharacters are ?, *, [ and \.
 * (And, inside character classes, !, - and ].)
 *
 * This is small and simple implementation intended for device blacklists
 * where a string is matched against a number of patterns.  Thus, it
 * does not preprocess the patterns.  It is non-recursive, and run-time
 * is at most quadratic: strlen(@str)*strlen(@pat).
 *
 * An example of the worst case is glob_match("*aaaaa", "aaaaaaaaaa");
 * it takes 6 passes over the pattern before matching the string.
 *
 * Like !fnmatch(@pat, @str, 0) and unlike the shell, this does NOT
 * treat / or leading . specially; it isn't actually used for pathnames.
 *
 * Note that according to glob(7) (and unlike bash), character classes
 * are complemented by a leading !; this does not support the regex-style
 * [^a-z] syntax.
 *
 * An opening bracket without a matching close is matched literally.
 */
bool
glob_match(char const *pat, char const *str, const char **a, const char **b)
{
	/*
	 * Backtrack to previous * on mismatch and retry starting one
	 * character later in the string.  Because * matches all characters
	 * (no exception for /), it can be easily proved that there's
	 * never a need to backtrack multiple levels.
	 */
	char const *back_pat = NULL, *back_str;
	if( a ) *a=NULL;
	/*
	 * Loop over each token (character or class) in pat, matching
	 * it against the remaining unmatched tail of str.  Return false
	 * on mismatch, or true after matching the trailing nul bytes.
	 */
	for (;;) {
		unsigned char c = *str++;
		unsigned char d = *pat++;

		switch (d) {
		case '(': /* start expression */
			if (a)
				*a = --str;
			break;
		case ')': /* stop expr */
			if (b)
				*b = --str;
			break;	
		case '?': /* Wildcard: anything but nul */
			if (c == '\0')
				return false;
			break;
		case '*':                 /* Any-length wildcard */
			if (*pat == '\0') /* Optimize trailing * case */
				return true;
			back_pat = pat;
			back_str = --str; /* Allow zero-length match */
			break;
		case '[': { /* Character class */
			bool match = false, inverted = (*pat == '!');
			char const *class = pat + inverted;
			unsigned char a = *class ++;

			/*
			 * Iterate over each span in the character class.
			 * A span is either a single character a, or a
			 * range a-b.  The first span may begin with ']'.
			 */
			do {
				unsigned char b = a;

				if (a == '\0') /* Malformed */
					goto literal;

				if (class[0] == '-' && class[1] != ']') {
					b = class[1];

					if (b == '\0')
						goto literal;

					class += 2;
					/* Any special action if a > b? */
				}
				match |= (a <= c && c <= b);
			} while ((a = *class ++) != ']');

			if (match == inverted)
				goto backtrack;
			pat = class;
		} break;
		case '\\':
			d = *pat++;
			/* fallthrough */
		default: /* Literal character */
		literal:
			if (c == d) {
				if (d == '\0')
					return true;
				break;
			}
		backtrack:
			if (c == '\0' || !back_pat)
				return false; /* No point continuing */
			/* Try again from last *, one character later in str. */
			pat = back_pat;
			str = ++back_str;
			break;
		}
	}
}
