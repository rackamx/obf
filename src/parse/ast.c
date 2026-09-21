/**
 * @file ast.c
 * @brief Abstract syntax tree nodes and declarator helpers.
 */

#include "parse/ast.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Allocate a zeroed AST node.
 *
 * @param t Node type (N_*).
 *
 * @return New node.
 */
Node *node_new(int t)
{
	Node *n = (Node *)xmalloc(sizeof(Node));

	memset(n, 0, t > 0 ? sizeof(Node) : sizeof(Node));
	n->type = t;
	return n;
}

/**
 * @brief Initialise an empty node vector.
 *
 * @param v Vector to initialise.
 */
void nv_init(NodeVec *v)
{
	v->items = NULL;
	v->len = 0;
	v->cap = 0;
}

/**
 * @brief Append a node.
 *
 * @param v Vector.
 * @param n Node to append.
 */
void nv_push(NodeVec *v, Node *n)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 8;

		v->items = (Node **)xrealloc(v->items, nc * sizeof(Node *));
		v->cap = nc;
	}

	v->items[v->len++] = n;
}

/**
 * @brief Append a declarator entry.
 *
 * @param v Vector.
 * @param e Entry to append (copied).
 */
void dv_push(DeclVec *v, DeclEnt e)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 4;

		v->items = (DeclEnt *)xrealloc(v->items, nc * sizeof(DeclEnt));
		v->cap = nc;
	}

	v->items[v->len++] = e;
}

/**
 * @brief Find the declared name in declarator tokens.
 *
 * @param toks Declarator tokens.
 * @param n Token count.
 *
 * @return Name into @p toks, or NULL.
 */
const char *extract_decl_name(Token *toks, size_t n)
{
	/* Gather every identifier outside brackets with its paren depth.
	 * Bracketed sizes such as "[10]" can never hold the name. */
	/**
	 * @brief Identifier candidate (function-local).
	 */
	typedef struct {
		int d;	       /**< Paren depth. */
		const char *t; /**< Spelling (borrowed). */
	} C;

	C *cs = (C *)xmalloc((n + 1) * sizeof(C));
	size_t cn = 0;
	int dp = 0, db = 0;

	for (size_t i = 0; i < n; i++) {
		const char *tx = toks[i].text;

		if (streq(tx, "("))
			dp++;
		else if (streq(tx, ")"))
			dp--;
		else if (streq(tx, "["))
			db++;
		else if (streq(tx, "]"))
			db--;
		else if (toks[i].kind == TOK_IDENT && db == 0) {
			cs[cn].d = dp;
			cs[cn].t = tx;
			cn++;
		}
	}

	const char *res = NULL;

	if (cn == 0) {
		free(cs);
		return NULL;
	}

	/* Depth-0 candidates win: they are the declarator itself rather
	 * than parameter names.  Take the last one, e.g. the "q" in
	 * "int *p, *q". */
	int haszero = 0;

	for (size_t i = 0; i < cn; i++)
		if (cs[i].d == 0)
			haszero = 1;
	if (haszero) {
		for (size_t i = cn; i > 0; i--)
			if (cs[i - 1].d == 0) {
				res = cs[i - 1].t;
				break;
			}
	} else {
		/* No depth-0 name: the declarator hides in parens, as in
		 * "(*f)(int)".  Then the first candidate is the name and
		 * the rest are parameter names. */
		if (n > 0 && streq(toks[0].text, "("))
			res = cs[0].t;
		else
			res = cs[cn - 1].t;
	}

	free(cs);
	return res;
}

/**
 * @brief Collect trailing '[...]' groups after a name.
 *
 * @param toks Declarator tokens.
 * @param n Token count.
 * @param name Declared name.
 *
 * @return New string, possibly empty.
 */
char *extract_decl_suffix(Token *toks, size_t n, const char *name)
{
	if (!name)
		return xstrdup("");

	int *dps = (int *)xmalloc(n * sizeof(int));
	int *dbs = (int *)xmalloc(n * sizeof(int));
	int dp = 0, db = 0;

	/* Record the nesting depth at each token; depths are read off
	 * after the token that changed them, which is close enough to
	 * locate the name. */
	for (size_t i = 0; i < n; i++) {
		if (streq(toks[i].text, "("))
			dp++;
		else if (streq(toks[i].text, ")"))
			dp--;
		else if (streq(toks[i].text, "["))
			db++;
		else if (streq(toks[i].text, "]"))
			db--;
		dps[i] = dp;
		dbs[i] = db;
	}

	int idx = -1;

	/* Search backwards so shadowing or repeated names resolve to the
	 * declarator rather than an earlier lookalike. */
	for (int k = (int)n - 1; k >= 0; k--)
		if (toks[k].kind == TOK_IDENT && streq(toks[k].text, name) &&
		    dps[k] == 0) {
			idx = k;
			break;
		}

	/* Fall back to any depth, e.g. names inside "(*f)". */
	if (idx < 0)
		for (size_t k = 0; k < n; k++)
			if (toks[k].kind == TOK_IDENT &&
			    streq(toks[k].text, name)) {
				idx = (int)k;
				break;
			}

	free(dps);
	free(dbs);
	if (idx < 0)
		return xstrdup("");
	StrBuf b;

	sb_init(&b);
	int first = 1;
	size_t k = (size_t)idx + 1;

	/* Collect each "[...]" group trailing the name, e.g. both halves
	 * of "a[3][4]". */
	while (k < n && streq(toks[k].text, "[")) {
		StrBuf g;

		sb_init(&g);
		int dd = 0;

		while (k < n) {
			if (!first || g.len)
				sb_putc(&g, ' ');

			/* avoid leading space */
			sb_puts(&g, toks[k].text);
			if (streq(toks[k].text, "["))
				dd++;
			else if (streq(toks[k].text, "]")) {
				dd--;
				if (dd == 0) {
					k++;
					break;
				}
			}

			k++;
		}

		char *gs = g.data;

		char *tgs = gs;

		while (*tgs == ' ')
			tgs++;
		if (!first)
			sb_putc(&b, ' ');
		sb_puts(&b, tgs);
		first = 0;
		free(gs);
	}

	/* Spacing such as "[ 3 ]" is kept; fix_array_suffix tolerates it. */
	return b.data;
}

/**
 * @brief Count pointer stars before a name.
 *
 * @param toks Declarator tokens.
 * @param n Token count.
 * @param name Declared name.
 *
 * @return New string such as "* *", possibly empty.
 */
char *extract_decl_stars(Token *toks, size_t n, const char *name)
{
	if (!name)
		return xstrdup("");
	size_t idx = n;

	/* First occurrence wins: the stars precede the name. */
	for (size_t k = 0; k < n; k++)
		if (toks[k].kind == TOK_IDENT && streq(toks[k].text, name)) {
			idx = k;
			break;
		}

	if (idx >= n)
		return xstrdup("");
	int cnt = 0;

	for (size_t k = 0; k < idx; k++)
		if (streq(toks[k].text, "*"))
			cnt++;
	StrBuf b;

	sb_init(&b);
	for (int i = 0; i < cnt; i++) {
		if (i)
			sb_putc(&b, ' ');
		sb_putc(&b, '*');
	}

	return b.data;
}
