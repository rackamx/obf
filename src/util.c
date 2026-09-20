/**
 * @file util.c
 * @brief Allocation helpers, string buffers and vectors.
 */

#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- utilities ---------------- */

/**
 * @brief Allocate memory, aborting on failure.
 *
 * @param n Number of bytes to allocate (0 still allocates).
 *
 * @return Pointer to the allocated memory.
 */
void *xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);

	if (!p) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	return p;
}

/**
 * @brief Resize a memory block, aborting on failure.
 *
 * @param p Block to resize, as in realloc().
 * @param n New size in bytes.
 *
 * @return Pointer to the resized block.
 */
void *xrealloc(void *p, size_t n)
{
	void *q = realloc(p, n ? n : 1);

	if (!q) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	return q;
}

/**
 * @brief Duplicate a string.
 *
 * @param s String to copy; may be NULL.
 *
 * @return New copy, or NULL when @p s is NULL.
 */
char *xstrdup(const char *s)
{
	if (!s)
		return NULL;
	size_t n = strlen(s) + 1;
	char *p = (char *)xmalloc(n);

	memcpy(p, s, n);
	return p;
}

/**
 * @brief Duplicate a counted prefix of a string plus NUL.
 *
 * @param s String to copy.
 * @param n Bytes to copy.
 *
 * @return New NUL-terminated string.
 */
char *xstrndup(const char *s, size_t n)
{
	char *p = (char *)xmalloc(n + 1);

	memcpy(p, s, n);
	p[n] = '\0';
	return p;
}

/* growable string buffer */

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
	if (b->len + extra + 1 > b->cap) {
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

/* vector of strings (owned) */

/**
 * @brief Initialise an empty string vector.
 *
 * @param v Vector to initialise.
 */
void sv_init(StrVec *v)
{
	v->items = NULL;
	v->len = 0;
	v->cap = 0;
}

/**
 * @brief Append a string, taking ownership of the pointer.
 *
 * @param v Vector.
 * @param s String now owned by the vector.
 */
void sv_push(StrVec *v, char *s)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 8;

		v->items = (char **)xrealloc(v->items, nc * sizeof(char *));
		v->cap = nc;
	}

	v->items[v->len++] = s;
}

/**
 * @brief Test membership by string equality.
 *
 * @param v Vector.
 * @param s String to look up.
 *
 * @return 1 when present, 0 otherwise.
 */
int sv_contains(StrVec *v, const char *s)
{
	for (size_t i = 0; i < v->len; i++)
		if (strcmp(v->items[i], s) == 0)
			return 1;
	return 0;
}

/**
 * @brief Compare two strings for equality.
 *
 * @param a First string.
 * @param b Second string.
 *
 * @return 1 when equal, 0 otherwise.
 */
int streq(const char *a, const char *b)
{
	return strcmp(a, b) == 0;
}
