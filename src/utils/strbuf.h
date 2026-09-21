/**
 * @file strbuf.h
 * @brief Growable string buffer.
 */

#ifndef CFLATTEN_STRBUF_H
#define CFLATTEN_STRBUF_H

#include <stddef.h>

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

#endif /* CFLATTEN_STRBUF_H */
