/**
 * @file parser.c
 * @brief C statement-level parser: tokenizer and AST builder.
 */

#include "parse/parser.h"
#include "parse/ast.h"
#include "parse/token.h"
#include "utils/strbuf.h"
#include "utils/strvec.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Statement-level recursive descent: each parse_* below handles one
 * starting keyword or construct. */

/**
 * @brief Look ahead without consuming input.
 *
 * @param p Parser.
 * @param k Lookahead offset.
 *
 * @return Token pointer, or NULL past the end.
 */
Token *p_peek(Parser *p, size_t k)
{
	size_t q = p->pos + k;

	return q < p->n ? &p->toks[q] : NULL;
}

/**
 * @brief Read ahead token text.
 *
 * @param p Parser.
 * @param k Lookahead offset.
 *
 * @return Token text, or NULL past the end.
 */
const char *p_peekt(Parser *p, size_t k)
{
	Token *t = p_peek(p, k);

	return t ? t->text : NULL;
}

/**
 * @brief Consume and return the current token.
 *
 * @param p Parser.
 *
 * @return Current token, or NULL at end.
 */
Token *p_next(Parser *p)
{
	return p->pos < p->n ? &p->toks[p->pos++] : NULL;
}

/**
 * @brief Test for end of input.
 *
 * @param p Parser.
 *
 * @return Non-zero at end of input.
 */
int p_eof(Parser *p)
{
	return p->pos >= p->n;
}

/**
 * @brief Consume the current token on text match.
 *
 * @param p Parser.
 * @param t Expected text.
 *
 * @return 1 on match, 0 otherwise.
 */
int p_expect(Parser *p, const char *t)
{
	Token *x = p_next(p);

	if (!x || !streq(x->text, t))
		return 0;
	return 1;
}

/**
 * @brief Test whether a word is a known typedef name.
 *
 * @param p Parser.
 * @param w Word to test.
 *
 * @return 1 when known, 0 otherwise.
 */
int parser_is_typedef_name(Parser *p, const char *w)
{
	for (size_t i = 0; i < p->typedefs->len; i++)
		if (streq(p->typedefs->items[i], w))
			return 1;
	return 0;
}

/**
 * @brief Heuristic declaration test at the cursor.
 *
 * @param p Parser.
 *
 * @return Non-zero for declarations.
 */
int looks_like_decl(Parser *p)
{
	Token *t0 = p_peek(p, 0);

	if (!t0)
		return 0;
	if (streq(t0->text, "typedef"))
		return 1;
	if (streq(t0->text, "struct") || streq(t0->text, "union") ||
	    streq(t0->text, "enum"))
		return 1;
	if (t0->kind == TOK_KEYWORD && is_type_kw(t0->text))
		return 1;
	if (t0->kind == TOK_IDENT && parser_is_typedef_name(p, t0->text))
		return 1;
	return 0;
}

/**
 * @brief Collect tokens up to ';' at nesting depth 0.
 *
 * @param p Parser.
 *
 * @return Inner tokens; the ';' is consumed.
 */
TokVec capture_until_semi(Parser *p)
{
	TokVec v;

	tv_init(&v);
	int dp = 0, db = 0, dc = 0;

	/* Copy everything, tracking ()/[]/{} depth so only a depth-0
	 * ';' ends the statement.  Initializer braces and calls with
	 * embedded semicolons survive intact. */
	while (!p_eof(p)) {
		Token *t = p_next(p);

		if (streq(t->text, "(")) {
			dp++;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ")")) {
			dp--;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "[")) {
			db++;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "]")) {
			db--;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "{")) {
			dc++;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "}")) {
			dc--;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ";") && dp == 0 && db == 0 &&

			   dc == 0) {
			break;
		} else

			tv_push(&v, t->kind, xstrdup(t->text));
	}

	return v;
}

/**
 * @brief Parse a brace-delimited statement list.
 *
 * @param p Parser.
 * @param need_braces Non-zero when on '{'.
 *
 * @return New block node.
 */
Node *parse_block_contents(Parser *p, int need_braces)
{
	if (need_braces) {
		if (!p_expect(p, "{"))
			return NULL;
	}

	Node *b = node_new(N_BLOCK);

	nv_init(&b->stmts);
	/* A '}' ends the block; anything else must be a statement.  A
	 * missing closer simply ends input handling below. */
	while (!p_eof(p)) {
		const char *t = p_peekt(p, 0);

		if (t && streq(t, "}"))
			break;
		Node *s = parse_statement(p);

		if (s)
			nv_push(&b->stmts, s);
	}

	if (need_braces) {
		const char *t = p_peekt(p, 0);

		/* Tolerate the missing brace: leaving it consumes nothing
		 * and lets the caller carry on. */
		if (t && streq(t, "}"))
			p_next(p);
	}

	return b;
}

/**
 * @brief Capture inside already-opened parens.
 *
 * @param p Parser.
 *
 * @return New string; closing ')' is consumed.
 */
char *parse_paren_inner_str(Parser *p)
{
	/* The '(' was already consumed by the caller; collect up to its
	 * matching ')' with nesting, then render the inside as text. */
	TokVec v;

	tv_init(&v);
	int depth = 1;

	while (!p_eof(p)) {
		Token *t = p_next(p);

		if (streq(t->text, "(")) {
			depth++;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ")")) {
			depth--;
			if (depth == 0)
				break;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else

			tv_push(&v, t->kind, xstrdup(t->text));
	}

	char *s = tokvec_to_str(&v);

	tv_free(&v);
	return s;
}

/**
 * @brief Parse an if/else statement at the cursor.
 *
 * @param p Parser.
 *
 * @return New if node.
 */
Node *parse_if(Parser *p)
{
	p_next(p);
	p_expect(p, "(");
	char *cond = parse_paren_inner_str(p);
	Node *th = parse_statement(p);
	Node *el = NULL;
	const char *t = p_peekt(p, 0);

	if (t && streq(t, "else")) {
		p_next(p);
		el = parse_statement(p);
	}

	Node *n = node_new(N_IF);

	n->cond = cond;
	n->then_b = th;
	n->else_b = el;
	return n;
}

/**
 * @brief Parse a while statement at the cursor.
 *
 * @param p Parser.
 *
 * @return New while node.
 */
Node *parse_while(Parser *p)
{
	p_next(p);
	p_expect(p, "(");
	char *cond = parse_paren_inner_str(p);
	Node *b = parse_statement(p);
	Node *n = node_new(N_WHILE);

	n->wcond = cond;
	n->body = b;
	return n;
}

/**
 * @brief Parse a for statement, detecting decl/expr init.
 *
 * @param p Parser.
 *
 * @return New for node.
 */
Node *parse_for(Parser *p)
{
	p_next(p);
	p_expect(p, "(");
	TokVec a;
	TokVec b;
	TokVec c;

	tv_init(&a);
	tv_init(&b);
	tv_init(&c);
	TokVec *cur = &a;
	int idx = 0, d = 0;

	/* Split the "(...)" into init/cond/incr at depth-0 ';'.  The final
	 * ')' closes the header when paren depth is back to zero. */
	while (!p_eof(p)) {
		Token *t = p_next(p);

		if (streq(t->text, "(")) {
			d++;
			tv_push(cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ")")) {
			if (d == 0)
				break;
			d--;
			tv_push(cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ";") && d == 0) {
			idx++;
			cur = (idx == 1) ? &b : &c;
			if (idx > 2)
				cur = &c;
		} else

			tv_push(cur, t->kind, xstrdup(t->text));
	}

	char *sa = tokvec_to_str(&a), *sb2 = tokvec_to_str(&b),
	     *sc = tokvec_to_str(&c);
	tv_free(&a);
	tv_free(&b);
	tv_free(&c);

	char *init_s = sa, *cond_s = sb2, *incr_s = sc;
	char *init_decl = NULL, *init_expr = NULL;

	/* An init starting with a type is "for (int i = ...)", anything
	 * else is a plain expression init.  Re-tokenize to look. */
	if (init_s[0]) {
		TokVec it = tokenize(init_s);
		int isd = 0;

		if (it.len > 0) {
			Token *t0 = &it.items[0];

			if (t0->kind == TOK_KEYWORD && is_type_kw(t0->text))
				isd = 1;
			else if (streq(t0->text, "typedef"))
				isd = 1;
			else if (streq(t0->text, "struct") ||
				 streq(t0->text, "union") ||
				 streq(t0->text, "enum"))
				isd = 1;
			else if (t0->kind == TOK_IDENT &&
				 parser_is_typedef_name(p, t0->text))
				isd = 1;
		}

		tv_free(&it);
		if (isd)
			init_decl = init_s;
		else
			init_expr = init_s;
	}

	Node *body = parse_statement(p);
	Node *n = node_new(N_FOR);

	n->init_decl = init_decl;
	n->init_expr = init_expr;

	/* Only one of the two above is ever set; when the init is empty
	 * there is nothing to free or keep. */
	if (!init_decl && !init_expr) {
		free(init_s);
	}

	n->for_cond = cond_s;
	n->incr = incr_s;
	n->for_body = body;

	return n;
}

/**
 * @brief Parse a do/while statement at the cursor.
 *
 * @param p Parser.
 *
 * @return New do-while node.
 */
Node *parse_do(Parser *p)
{
	p_next(p);
	Node *body = parse_statement(p);
	const char *t = p_peekt(p, 0);

	if (t && streq(t, "while"))
		p_next(p);
	else {
		/* Lone "do" without "while": loop forever. */
		Node *n = node_new(N_DOWHILE);

		n->body = body;
		n->wcond = xstrdup("1");
		return n;
	}

	p_expect(p, "(");
	char *cond = parse_paren_inner_str(p);
	const char *s = p_peekt(p, 0);

	if (s && streq(s, ";"))
		p_next(p);
	Node *n = node_new(N_DOWHILE);

	n->body = body;
	n->wcond = cond;
	return n;
}

/**
 * @brief Parse a switch statement at the cursor.
 *
 * @param p Parser.
 *
 * @return New switch node.
 */
Node *parse_switch(Parser *p)
{
	p_next(p);
	p_expect(p, "(");
	char *e = parse_paren_inner_str(p);
	Node *b = parse_statement(p);
	Node *n = node_new(N_SWITCH);

	n->sw_expr = e;
	n->sw_body = b;
	return n;
}

/**
 * @brief Parse a case label marker.
 *
 * @param p Parser.
 *
 * @return New case node.
 */
Node *parse_case(Parser *p)
{
	p_next(p);
	TokVec v;

	tv_init(&v);
	int d = 0;

	/* The value runs to ':'; a ';' also ends it so a missing colon
	 * cannot swallow the rest of the function. */
	while (!p_eof(p)) {
		Token *t = p_next(p);

		if (streq(t->text, "(") || streq(t->text, "[")) {
			d++;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ")") || streq(t->text, "]")) {
			d--;
			tv_push(&v, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ":") && d == 0)

			break;
		else if (streq(t->text, ";") && d == 0)
			break;
		else
			tv_push(&v, t->kind, xstrdup(t->text));
	}

	char *e = tokvec_to_str(&v);

	tv_free(&v);
	Node *n = node_new(N_CASE);

	/* Only the marker is returned here; following statements are
	 * parsed as siblings by the caller. */
	n->case_expr = e;
	return n;
}

/**
 * @brief Parse a default label marker.
 *
 * @param p Parser.
 *
 * @return New default node.
 */
Node *parse_default(Parser *p)
{
	p_next(p);
	const char *t = p_peekt(p, 0);

	if (t && streq(t, ":"))
		p_next(p);
	return node_new(N_DEFAULT);
}

/**
 * @brief Parse a declaration up to ';'.
 *
 * @param p Parser.
 *
 * @return New node, or NULL when not a declaration.
 */
Node *parse_declaration(Parser *p)
{
	size_t start = p->pos;
	TokVec toks;

	tv_init(&toks);
	int dp = 0, db = 0, dc = 0;
	int is_td = 0;
	const char *pt = p_peekt(p, 0);

	if (pt && streq(pt, "typedef"))
		is_td = 1;
	int got_semi = 0;

	/* Grab the whole declaration through its terminating ';',
	 * honouring nested (), [] and {} such as calls in initializers
	 * or brace lists. */
	while (!p_eof(p)) {
		Token *t = p_next(p);

		tv_push(&toks, t->kind, xstrdup(t->text));
		if (streq(t->text, "("))
			dp++;
		else if (streq(t->text, ")"))
			dp--;
		else if (streq(t->text, "["))
			db++;
		else if (streq(t->text, "]"))
			db--;
		else if (streq(t->text, "{"))
			dc++;
		else if (streq(t->text, "}"))
			dc--;
		else if (streq(t->text, ";") && dp == 0 && db == 0 && dc == 0) {
			got_semi = 1;
			break;
		}
	}

	if (!got_semi) {
		/* Not a declaration after all: rewind so the caller can
		 * retry as an expression statement. */
		tv_free(&toks);
		p->pos = start;
		return NULL;
	}

	/* Everything but the trailing ';' is the declarator region. */
	size_t bn = toks.len - 1;

	if (bn == 0) {
		/* A lone ';' carries nothing. */
	}

	/* Commas at depth 0 separate declarators ("int a, b;"), while
	 * commas inside parens, brackets or braces belong to the
	 * declarator itself. */
	/**
	 * @brief Declarator chunk (function-local).
	 */
	typedef struct {
		Token *t;   /**< Tokens (borrowed slice). */
		size_t n;   /**< Token count. */
		size_t cap; /**< Allocated slots. */
	} Chunk;

	Chunk *chunks = NULL;
	size_t cn = 0, cc = 0;
	TokVec cur;

	tv_init(&cur);
	int d1 = 0, d2 = 0, d3 = 0;

	for (size_t i = 0; i < bn; i++) {
		Token *t = &toks.items[i];

		if (streq(t->text, "(")) {
			d1++;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ")")) {
			d1--;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "[")) {
			d2++;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "]")) {
			d2--;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "{")) {
			d3++;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, "}")) {
			d3--;
			tv_push(&cur, t->kind, xstrdup(t->text));
		} else if (streq(t->text, ",") && d1 == 0 && d2 == 0 &&

			   d3 == 0) {
			if (cn == cc) {
				cc = cc ? cc * 2 : 4;
				chunks = (Chunk *)xrealloc(chunks,
							   cc * sizeof(Chunk));
			}

			chunks[cn].t = cur.items;
			chunks[cn].n = cur.len;
			chunks[cn].cap = cur.cap;
			cn++;
			tv_init(&cur);
		} else

			tv_push(&cur, t->kind, xstrdup(t->text));
	}

	if (cn == cc) {
		cc = cc ? cc * 2 : 4;
		chunks = (Chunk *)xrealloc(chunks, cc * sizeof(Chunk));
	}

	chunks[cn].t = cur.items;
	chunks[cn].n = cur.len;
	chunks[cn].cap = cur.cap;
	cn++;
	if (is_td) {
		/* A typedef only teaches new type names: the last
		 * identifier of each chunk is the name being defined. */
		for (size_t i = 0; i < cn; i++) {
			const char *last = NULL;

			for (size_t k = 0; k < chunks[i].n; k++)
				if (chunks[i].t[k].kind == TOK_IDENT)
					last = chunks[i].t[k].text;
			if (last) {
				if (!sv_contains(p->typedefs, last))
					sv_push(p->typedefs, xstrdup(last));
			}
		}

		char *txt = tokvec_to_str(&toks);
		Node *n = node_new(N_TYPEDEF);

		n->decl_text = txt;
		tv_free(&toks);
		for (size_t i = 0; i < cn; i++) {
			for (size_t k = 0; k < chunks[i].n; k++)
				free(chunks[i].t[k].text);
			free(chunks[i].t);
		}

		free(chunks);
		return n;
	}

	/* Walk the first chunk to find where the type prefix ends and
	 * the declarator begins.  Type keywords, known typedef names and
	 * '*' extend the prefix; struct/union/enum swallow their optional
	 * name and brace body as well. */
	Token *first = chunks[0].t;
	size_t fn = chunks[0].n;
	size_t type_end = 0, i2 = 0;
	int fail = 0;

	while (i2 < fn) {
		Token *t = &first[i2];

		if (t->kind == TOK_KEYWORD && is_type_kw(t->text)) {
			if (streq(t->text, "struct") ||
			    streq(t->text, "union") || streq(t->text, "enum")) {
				i2++;
				if (i2 < fn && first[i2].kind == TOK_IDENT)
					i2++;
				if (i2 < fn && streq(first[i2].text, "{")) {
					int dd = 0;

					while (i2 < fn) {
						if (streq(first[i2].text, "{"))
							dd++;
						else if (streq(first[i2].text,
							       "}")) {
							dd--;
							if (dd == 0) {
								i2++;
								break;
							}
						}

						i2++;
					}
				}

				type_end = i2;
				continue;
			} else {
				i2++;
				type_end = i2;
				continue;
			}
		} else if (t->kind == TOK_IDENT &&

			   parser_is_typedef_name(p, t->text)) {
			i2++;
			type_end = i2;
			continue;
		} else if (streq(t->text, "*")) {
			/* A '*' right after the type belongs to this first
			 * declarator, e.g. the '*' in "int *p, q". */
			i2++;
			type_end = i2;
			continue;
		} else if (t->kind == TOK_IDENT && i2 == 0 && cn == 1) {
			/* Starts with an unknown identifier and consumed no
			 * type: an expression such as "x = 5", not a
			 * declaration. */
			fail = 1;
			break;
		} else

			break;
	}

	if (type_end == 0)
		fail = 1;
	if (fail) {
		/* Same rewind contract as the missing-';' case above. */
		tv_free(&toks);
		for (size_t i = 0; i < cn; i++) {
			for (size_t k = 0; k < chunks[i].n; k++)
				free(chunks[i].t[k].text);
			free(chunks[i].t);
		}

		free(chunks);
		p->pos = start;
		return NULL;
	}

	char *type_str = toks_to_str(first, type_end);

	/* A bare type with no declarator ("int;") declares nothing but
	 * is still passed through. */
	if (chunks[0].n == type_end && cn == 1) {
		char *txt = tokvec_to_str(&toks);
		Node *n = node_new(N_DECL_NOVAR);

		n->decl_text = txt;
		free(type_str);
		tv_free(&toks);
		for (size_t i = 0; i < cn; i++) {
			for (size_t k = 0; k < chunks[i].n; k++)
				free(chunks[i].t[k].text);
			free(chunks[i].t);
		}

		free(chunks);
		return n;
	}

	/* Remainder of the first chunk past the type, then every later
	 * chunk wholesale: each holds one "name = init" declarator. */
	Node *nd = node_new(N_DECL);

	nd->type_str = type_str;
	nd->decls.items = NULL;
	nd->decls.len = nd->decls.cap = 0;

	/* Borrowed token slices, one per declarator. */
	/**
	 * @brief Declarator token slice (function-local).
	 */
	typedef struct {
		Token *t; /**< First token (borrowed). */
		size_t n; /**< Token count. */
	} DC;

	DC *dcs = NULL;
	size_t dn = 0, dcap = 0;

	if (chunks[0].n > type_end) {
		if (dn == dcap) {
			dcap = dcap ? dcap * 2 : 4;
			dcs = (DC *)xrealloc(dcs, dcap * sizeof(DC));
		}

		dcs[dn].t = first + type_end;
		dcs[dn].n = chunks[0].n - type_end;
		dn++;
	}

	for (size_t i = 1; i < cn; i++) {
		if (dn == dcap) {
			dcap = dcap ? dcap * 2 : 4;
			dcs = (DC *)xrealloc(dcs, dcap * sizeof(DC));
		}

		dcs[dn].t = chunks[i].t;
		dcs[dn].n = chunks[i].n;
		dn++;
	}

	for (size_t i = 0; i < dn; i++) {
		Token *dc = dcs[i].t;
		size_t m = dcs[i].n;
		int a1 = 0, a2 = 0, a3 = 0;
		int eq = -1;

		/* The '=' splitting declarator from initializer must sit
		 * at depth 0; '=' inside parens, brackets or braces (a
		 * call argument, an array size, a brace list) is data. */
		for (size_t k = 0; k < m; k++) {
			if (streq(dc[k].text, "("))
				a1++;
			else if (streq(dc[k].text, ")"))
				a1--;
			else if (streq(dc[k].text, "["))
				a2++;
			else if (streq(dc[k].text, "]"))
				a2--;
			else if (streq(dc[k].text, "{"))
				a3++;
			else if (streq(dc[k].text, "}"))
				a3--;
			else if (streq(dc[k].text, "=") && a1 == 0 && a2 == 0 &&
				 a3 == 0) {
				eq = (int)k;
				break;
			}
		}

		Token *left = dc;
		size_t ln = (eq >= 0) ? (size_t)eq : m;
		Token *ini = (eq >= 0) ? dc + eq + 1 : NULL;
		size_t in_ = (eq >= 0) ? m - (size_t)eq - 1 : 0;
		char *left_s = toks_to_str(left, ln);
		char *init_s = (eq >= 0) ? toks_to_str(ini, in_) : NULL;
		const char *nm = extract_decl_name(left, ln);
		char *nmdup = nm ? xstrdup(nm) : NULL;
		DeclEnt e;

		memset(&e, 0, sizeof(e));
		if (!nmdup) {
			/* No variable name: a nested function prototype
			 * such as "foo(int)".  Kept verbatim. */
			e.name = NULL;
			e.left = left_s;
			e.init = init_s;
			e.raw = toks_to_str(dc, m);
			e.suffix = NULL;
			e.stars = NULL;
		} else {
			e.name = nmdup;
			e.left = left_s;
			e.init = init_s;
			e.suffix = extract_decl_suffix(left, ln, nm);
			e.stars = extract_decl_stars(left, ln, nm);
			e.raw = toks_to_str(dc, m);
		}

		dv_push(&nd->decls, e);
	}

	{
		char *txt = tokvec_to_str(&toks);

		nd->decl_text = txt;
	}

	tv_free(&toks);
	for (size_t i = 0; i < cn; i++) {
		for (size_t k = 0; k < chunks[i].n; k++)
			free(chunks[i].t[k].text);
		free(chunks[i].t);
	}

	free(chunks);
	free(dcs);
	return nd;
}

/**
 * @brief Parse one statement at the cursor.
 *
 * @param p Parser.
 *
 * @return New node, or NULL at end.
 */
Node *parse_statement(Parser *p)
{
	Token *t = p_peek(p, 0);

	if (!t)
		return NULL;
	const char *tx = t->text;

	if (streq(tx, "{"))
		return parse_block_contents(p, 1);
	if (streq(tx, ";")) {
		p_next(p);
		return node_new(N_EMPTY);
	}

	if (streq(tx, "if"))
		return parse_if(p);
	if (streq(tx, "while"))
		return parse_while(p);
	if (streq(tx, "for"))
		return parse_for(p);
	if (streq(tx, "do"))
		return parse_do(p);
	if (streq(tx, "switch"))
		return parse_switch(p);
	if (streq(tx, "case"))
		return parse_case(p);
	if (streq(tx, "default"))
		return parse_default(p);
	if (streq(tx, "break")) {
		p_next(p);
		const char *s = p_peekt(p, 0);

		if (s && streq(s, ";"))
			p_next(p);
		else {
			TokVec v = capture_until_semi(p);

			tv_free(&v);
		}

		return node_new(N_BREAK);
	}

	if (streq(tx, "continue")) {
		p_next(p);
		const char *s = p_peekt(p, 0);

		if (s && streq(s, ";"))
			p_next(p);
		else {
			TokVec v = capture_until_semi(p);

			tv_free(&v);
		}

		return node_new(N_CONTINUE);
	}

	if (streq(tx, "goto")) {
		p_next(p);
		Token *lb = p_next(p);
		char *ln = lb ? xstrdup(lb->text) : xstrdup("");
		const char *s = p_peekt(p, 0);

		if (s && streq(s, ";"))
			p_next(p);
		else {
			TokVec v = capture_until_semi(p);

			tv_free(&v);
		}

		Node *n = node_new(N_GOTO);

		n->label = ln;
		return n;
	}

	if (streq(tx, "return")) {
		p_next(p);
		const char *s = p_peekt(p, 0);

		if (s && streq(s, ";")) {
			p_next(p);
			Node *n = node_new(N_RETURN);

			n->ret_expr = NULL;
			return n;
		}

		TokVec v = capture_until_semi(p);
		char *e = tokvec_to_str(&v);

		tv_free(&v);

		Node *n = node_new(N_RETURN);

		/* The join may carry surrounding blanks; strip both ends. */
		while (*e && isspace((unsigned char)*e))
			memmove(e, e + 1, strlen(e));
		size_t L = strlen(e);

		while (L && isspace((unsigned char)e[L - 1]))
			e[--L] = '\0';
		n->ret_expr = e;
		return n;
	}

	if (streq(tx, "typedef")) {
		size_t save = p->pos;
		(void)save;

		p_next(p);
		TokVec v = capture_until_semi(p);
		const char *last = NULL;

		/* The new type name is the last identifier, as in
		 * "typedef struct { ... } T". */
		for (size_t i = 0; i < v.len; i++)
			if (v.items[i].kind == TOK_IDENT)
				last = v.items[i].text;
		if (last) {
			if (!sv_contains(p->typedefs, last))
				sv_push(p->typedefs, xstrdup(last));
		}

		char *inner = tokvec_to_str(&v);

		tv_free(&v);
		StrBuf b;

		sb_init(&b);
		sb_puts(&b, "typedef ");
		sb_puts(&b, inner);
		sb_puts(&b, " ;");
		free(inner);
		Node *n = node_new(N_TYPEDEF);

		n->decl_text = b.data;
		return n;
	}

	if (t->kind == TOK_IDENT) {
		const char *c1 = p_peekt(p, 1), *c2 = p_peekt(p, 2);

		/* "name :" is a label, but "::" belongs to C++ scope
		 * resolution and is not ours. */
		if (c1 && streq(c1, ":") && !(c2 && streq(c2, ":"))) {
			char *lname = xstrdup(t->text);

			p_next(p);
			p_next(p);
			const char *nx = p_peekt(p, 0);

			if (!nx || streq(nx, "}")) {
				/* Label with nothing after it. */
				Node *n = node_new(N_LABEL);

				n->label = lname;
				return n;
			}

			if (p_eof(p)) {
				Node *n = node_new(N_LABEL);

				n->label = lname;
				return n;
			}

			/* Otherwise the label prefixes a statement parsed
			 * right away and wrapped with it. */
			Node *inner = parse_statement(p);
			Node *n = node_new(N_LABELED);

			n->label = lname;
			n->labeled_stmt = inner;
			return n;
		}
	}

	if (looks_like_decl(p)) {
		size_t save = p->pos;
		Node *d = parse_declaration(p);

		if (d)
			return d;
		/* Declaration parse failed: rewind and fall through to
		 * the expression case below. */
		p->pos = save;
	}

	TokVec v = capture_until_semi(p);

	if (v.len == 0) {
		tv_free(&v);
		return node_new(N_EMPTY);
	}

	char *s = tokvec_to_str(&v);

	tv_free(&v);

	{
		int blank = 1;

		for (char *q = s; *q; q++)
			if (!isspace((unsigned char)*q)) {
				blank = 0;
				break;
			}

		/* Whitespace-only text is an empty statement. */
		if (blank) {
			free(s);
			return node_new(N_EMPTY);
		}
	}

	Node *n = node_new(N_EXPR);
	StrBuf b;

	sb_init(&b);
	sb_puts(&b, s);
	sb_puts(&b, " ;");
	free(s);
	/* Kept with its ';' so lowering can emit it verbatim. */
	n->expr_text = b.data;
	return n;
}
