/**
 * @file hoister.h
 * @brief Declaration hoisting and scope renaming.
 */

#ifndef CFLATTEN_HOISTER_H
#define CFLATTEN_HOISTER_H

#include "parse/ast.h"
#include "utils/strvec.h"

/**
 * @brief Single name renaming.
 */
typedef struct {
	char *orig; /**< Source name. */
	char *neww; /**< Hoisted name. */
} MapEnt;

/**
 * @brief Renaming vector.
 */
typedef struct {
	MapEnt *items; /**< Entries. */
	size_t len;    /**< Item count. */
	size_t cap;    /**< Allocated slots. */
} MapVec;

/**
 * @brief Stack of renaming scopes.
 */
typedef struct {
	MapVec *scopes; /**< Scopes, innermost last. */
	size_t len;	/**< Depth. */
	size_t cap;	/**< Allocated slots. */
} ScopeStack;

/**
 * @brief Declaration hoister and renamer.
 */
typedef struct {
	StrVec hoisted;	      /**< Hoisted declarations. */
	StrVec hoisted_names; /**< Used hoisted names. */
	MapVec *stack;	      /**< Renaming scopes. */
	size_t slen, scap;    /**< Scope capacity. */
	int counter;	      /**< Fresh-name counter. */
	int temp_counter;     /**< Init-temporary counter. */
	StrVec *typedefs;     /**< Known typedef names (borrowed). */
} Hoister;

void hoister_init(Hoister *h, StrVec *td);

void h_push(Hoister *h);

void h_pop(Hoister *h);

char *h_fresh(Hoister *h, const char *base);

int h_in_hoisted(Hoister *h, const char *s);

int h_shadowed(Hoister *h, const char *s);

const char *h_lookup(Hoister *h, const char *s);

char *h_declare(Hoister *h, const char *orig, const char *type_str,
		const char *stars, const char *suffix, const char *init_text);

char *rewrite_expr(Hoister *h, const char *expr);

int contains_word(const char *s, const char *w);

char *strip_const_word(const char *s);

int is_struct_type(const char *t);

int c_str_lit_size(const char *lit);

char *replace_first_empty_brackets(const char *suffix, int size);

char *fix_array_suffix(const char *suffix, const char *init_text);

Node *parse_decl_string(Hoister *h, const char *s);

NodeVec make_array_init(Hoister *h, const char *new_name,
			const char *init_text);

void nv_extend(NodeVec *dst, NodeVec *src);

NodeVec hoist_node(Hoister *h, Node *nd);

#endif /* CFLATTEN_HOISTER_H */
