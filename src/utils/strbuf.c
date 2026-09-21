/**
 * @file strbuf.c
 * @brief Growable string buffer.
 */

#include "utils/strbuf.h"
#include "utils/util.h"
#include <string.h>

/* ---------------- string buffer ---------------- */

/**
 * @brief Initialise an empty string buffer.
 *
 * @param b Buffer to initialise.
 */
void sb_init(StrBuf *b)
{
	b->data = (char *)xmalloc(256);
	b->data[0] = '\0';
	b->len = 0;
	b->cap = 256;
}

/**
 * @brief Ensure room for extra bytes.
 *
 * @param b Buffer.
 * @param extra Bytes to fit beyond current length.
 */
void sb_reserve(StrBuf *b, size_t extra)
{
	/* Plus one for the NUL we always keep after the payload. */
	if (b->len + extra + 1 > b->cap) {
		/* Double, with slack so tiny appends don't realloc each time.
		 */
		size_t nc = b->cap * 2 + extra + 64;

		b->data = (char *)xrealloc(b->data, nc);
		b->cap = nc;
	}
}

/**
 * @brief Append counted bytes to a buffer.
 *
 * @param b Buffer.
 * @param s Bytes to append.
 * @param n Byte count.
 */
void sb_putn(StrBuf *b, const char *s, size_t n)
{
	sb_reserve(b, n);
	memcpy(b->data + b->len, s, n);
	b->len += n;
	/* Keep the buffer a valid C string at all times. */
	b->data[b->len] = '\0';
}

/**
 * @brief Append a NUL-terminated string; NULL-safe.
 *
 * @param b Buffer.
 * @param s String to append, or NULL for no-op.
 */
void sb_puts(StrBuf *b, const char *s)
{
	if (s)
		sb_putn(b, s, strlen(s));
}

/**
 * @brief Append a single character.
 *
 * @param b Buffer.
 * @param c Character to append.
 */
void sb_putc(StrBuf *b, char c)
{
	sb_reserve(b, 1);
	b->data[b->len++] = c;
	b->data[b->len] = '\0';
}
