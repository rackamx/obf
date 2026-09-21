/**
 * @file strvec.h
 * @brief Growable vector of owned strings.
 */

#ifndef CFLATTEN_STRVEC_H
#define CFLATTEN_STRVEC_H

#include <stddef.h>

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

#endif /* CFLATTEN_STRVEC_H */
