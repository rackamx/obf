/**
 * @file util.h
 * @brief Allocation helpers and string comparison.
 */

/* Shared utilities: allocation helpers and string comparison. */

#ifndef CFLATTEN_UTIL_H
#define CFLATTEN_UTIL_H

#include <stddef.h>

void *xmalloc(size_t n);

void *xrealloc(void *p, size_t n);

char *xstrdup(const char *s);

char *xstrndup(const char *s, size_t n);

int streq(const char *a, const char *b);

#endif /* CFLATTEN_UTIL_H */
