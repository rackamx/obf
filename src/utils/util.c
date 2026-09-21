/**
 * @file util.c
 * @brief Allocation helpers and string comparison.
 */

#include "utils/util.h"
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

/* vector of strings (owned) */

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
