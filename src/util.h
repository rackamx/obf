/**
 * @file util.h
 * @brief Allocation helpers, string buffers and vectors.
 */
/* Shared utilities: allocation helpers, string buffers and vectors. */
#ifndef CFLATTEN_UTIL_H
#define CFLATTEN_UTIL_H
#include <stddef.h>
void *xmalloc(size_t n);

void *xrealloc(void *p, size_t n);

char *xstrdup(const char *s);

char *xstrndup(const char *s, size_t n);
/**
 * @brief Growable string buffer.
 */
typedef struct {
	char *data; /**< Storage (NUL-terminated). */
	size_t len; /**< Used length excluding NUL. */
	size_t cap; /**< Allocated capacity. */
} StrBuf;

void sb_init(StrBuf *b);

void sb_reserve(StrBuf *b, size_t extra);

void sb_putn(StrBuf *b, const char *s, size_t n);

void sb_puts(StrBuf *b, const char *s);

void sb_putc(StrBuf *b, char c);
/* vector of strings (owned) */
/**
 * @brief Growable vector of owned strings.
 */
typedef struct {
	char **items; /**< Owned strings. */
	size_t len;   /**< Item count. */
	size_t cap;   /**< Allocated slots. */
} StrVec;

void sv_init(StrVec *v);

void sv_push(StrVec *v, char *s);

int sv_contains(StrVec *v, const char *s);

int streq(const char *a, const char *b);
#endif /* CFLATTEN_UTIL_H */
