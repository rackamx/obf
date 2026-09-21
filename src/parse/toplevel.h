/**
 * @file toplevel.h
 * @brief Toplevel splitter: function definitions versus others.
 */

#ifndef CFLATTEN_TOPLEVEL_H
#define CFLATTEN_TOPLEVEL_H

#include "parse/token.h"

/**
 * @brief Toplevel declaration kinds.
 */
enum {
	TL_FUNC = 0, /**< Function definition. */
	TL_OTHER     /**< Any other toplevel text. */
};

/**
 * @brief Toplevel declaration.
 */
typedef struct {
	int kind;      /**< TL_FUNC or TL_OTHER. */
	char *text;    /**< Other text (NULL for functions). */
	TokVec header; /**< Function header tokens. */
	TokVec body;   /**< Braced body tokens. */
	char *name;    /**< Function name. */
} TLItem;

/**
 * @brief Toplevel item vector.
 */
typedef struct {
	TLItem *items; /**< Items. */
	size_t len;    /**< Item count. */
	size_t cap;    /**< Allocated slots. */
} TLVect;

void tlv_push(TLVect *v, TLItem it);

const char *is_func_header(Token *h, size_t n);

TLVect extract_toplevel(const char *code);

#endif /* CFLATTEN_TOPLEVEL_H */
