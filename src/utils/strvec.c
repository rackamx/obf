/**
 * @file strvec.c
 * @brief Growable vector of owned strings.
 */

#include "utils/strvec.h"
#include "utils/util.h"
#include <string.h>

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
	/* Amortised doubling; first growth starts at 8 slots. */
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
