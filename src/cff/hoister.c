/**
 * @file hoister.c
 * @brief Declaration hoisting and scope renaming.
 */

#include "cff/hoister.h"
#include "parse/parser.h"
#include "parse/token.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initialise a hoister with a base scope.
 *
 * @param h Hoister.
 * @param td Known typedef names.
 */
void hoister_init(Hoister *h, StrVec *td)
{
	sv_init(&h->hoisted);
	sv_init(&h->hoisted_names);
	h->stack = NULL;
	h->slen = 0;
	h->scap = 0;
	h->counter = 0;
	h->temp_counter = 0;
	h->typedefs = td;

	/* Function scope always exists, so lookups never run off the
	 * bottom of the stack. */
	h->stack = (MapVec *)xmalloc(sizeof(MapVec));
	h->stack[0].items = NULL;
	h->stack[0].len = 0;
	h->stack[0].cap = 0;
	h->slen = 1;
	h->scap = 1;
}

/**
 * @brief Push a renaming scope.
 *
 * @param h Hoister.
 */
void h_push(Hoister *h)
{
	if (h->slen == h->scap) {
		size_t nc = h->scap ? h->scap * 2 : 4;

		h->stack = (MapVec *)xrealloc(h->stack, nc * sizeof(MapVec));
		h->scap = nc;
	}

	h->stack[h->slen].items = NULL;
	h->stack[h->slen].len = 0;
	h->stack[h->slen].cap = 0;
	h->slen++;
}

/**
 * @brief Pop the innermost renaming scope.
 *
 * @param h Hoister.
 */
void h_pop(Hoister *h)
{
	if (h->slen > 0)
		h->slen--;
}

/**
 * @brief Derive a unique name from a base.
 *
 * @param h Hoister.
 * @param base Base name.
 *
 * @return New string.
 */
char *h_fresh(Hoister *h, const char *base)
{
	h->counter++;
	char buf[512];

	snprintf(buf, sizeof(buf), "%s__h%d", base, h->counter);
	return xstrdup(buf);
}

/**
 * @brief Test whether a name was already hoisted.
 *
 * @param h Hoister.
 * @param s Name.
 *
 * @return 1 when present.
 */
int h_in_hoisted(Hoister *h, const char *s)
{
	return sv_contains(&h->hoisted_names, s);
}

/**
 * @brief Test whether a name is bound in any scope.
 *
 * @param h Hoister.
 * @param s Name.
 *
 * @return 1 when shadowed.
 */
int h_shadowed(Hoister *h, const char *s)
{
	for (size_t i = 0; i < h->slen; i++) {
		MapVec *m = &h->stack[i];

		for (size_t k = 0; k < m->len; k++)
			if (streq(m->items[k].orig, s))
				return 1;
	}

	return 0;
}

/**
 * @brief Resolve the innermost renaming of a name.
 *
 * @param h Hoister.
 * @param s Name.
 *
 * @return Renamed or original pointer; do not free.
 */
const char *h_lookup(Hoister *h, const char *s)
{
	for (size_t i = h->slen; i > 0; i--) {
		MapVec *m = &h->stack[i - 1];

		for (size_t k = 0; k < m->len; k++)
			if (streq(m->items[k].orig, s))
				return m->items[k].neww;
	}

	return s;
}

/**
 * @brief Hoist a variable, keeping static initializers inline.
 *
 * @param h Hoister.
 * @param orig Source name.
 * @param type_str Base type text.
 * @param stars Pointer stars.
 * @param suffix Array suffix.
 * @param init_text Initializer, or NULL.
 *
 * @return New, possibly renamed, variable name.
 */
char *h_declare(Hoister *h, const char *orig, const char *type_str,
		const char *stars, const char *suffix, const char *init_text)
{
	char *neww = NULL;

	/* A name already hoisted, or bound in an outer scope, would collide
	 * or shadow: mint "name__hN" instead.  First declarations keep
	 * their spelling so output stays readable. */
	if (h_in_hoisted(h, orig) || h_shadowed(h, orig))
		neww = h_fresh(h, orig);
	else
		neww = xstrdup(orig);
	while (h_in_hoisted(h, neww)) {
		free(neww);
		neww = h_fresh(h, orig);
	}

	sv_push(&h->hoisted_names, xstrdup(neww));
	MapVec *top = &h->stack[h->slen - 1];

	if (top->len == top->cap) {
		size_t nc = top->cap ? top->cap * 2 : 4;

		top->items =
			(MapEnt *)xrealloc(top->items, nc * sizeof(MapEnt));

		top->cap = nc;
	}

	top->items[top->len].orig = xstrdup(orig);
	top->items[top->len].neww = xstrdup(neww);
	top->len++;
	StrBuf b;

	/* Rebuild the declaration around the new name.  An attached
	 * initializer survives only for statics (see hoist_node); plain
	 * locals are assigned at their original spot instead. */
	sb_init(&b);
	sb_puts(&b, type_str);
	sb_putc(&b, ' ');
	if (stars && stars[0]) {
		sb_puts(&b, stars);
		sb_putc(&b, ' ');
	}

	sb_puts(&b, neww);
	if (suffix && suffix[0]) {
		sb_putc(&b, ' ');
		sb_puts(&b, suffix);
	}

	if (init_text) {
		sb_puts(&b, " = ");
		sb_puts(&b, init_text);
	}

	sb_puts(&b, " ;");
	sv_push(&h->hoisted, b.data);
	return neww;
}

/**
 * @brief Rename declared identifiers, skipping fields/tags.
 *
 * @param h Hoister.
 * @param expr Expression text.
 *
 * @return New string.
 */
char *rewrite_expr(Hoister *h, const char *expr)
{
	if (!expr || !expr[0])
		return xstrdup(expr ? expr : "");
	TokVec v = tokenize(expr);
	StrBuf b;

	sb_init(&b);
	const char *prev = NULL;

	for (size_t i = 0; i < v.len; i++) {
		if (i)
			sb_putc(&b, ' ');
		Token *t = &v.items[i];

		if (t->kind == TOK_IDENT) {
			/* Fields ("a.b"), pointed-to members ("p->x") and
			 * type tags ("struct S") live in their own
			 * namespaces: never rename them.  Anything else is
			 * looked up; undeclared names (globals, functions)
			 * come back unchanged. */
			if (prev &&
			    (streq(prev, ".") || streq(prev, "->") ||
			     streq(prev, "struct") || streq(prev, "union") ||
			     streq(prev, "enum")))
				sb_puts(&b, t->text);
			else if (is_keyword(t->text))
				sb_puts(&b, t->text);
			else
				sb_puts(&b, h_lookup(h, t->text));
		} else

			sb_puts(&b, t->text);
		prev = t->text;
	}

	tv_free(&v);
	return b.data;
}

/**
 * @brief Test for a whole-word occurrence.
 *
 * @param s Haystack.
 * @param w Word.
 *
 * @return 1 when found.
 */
int contains_word(const char *s, const char *w)
{
	size_t wl = strlen(w);
	const char *p = s;

	/* Substring hits don't count: the char on each side must not
	 * extend the identifier ("x" in "xy" is no match). */
	while ((p = strstr(p, w))) {
		int left_ok = (p == s) || (!is_ident_char(p[-1]));
		int right_ok = !is_ident_char(p[wl]);

		if (left_ok && right_ok)
			return 1;
		p += wl;
	}

	return 0;
}

/**
 * @brief Drop 'const' words and collapse whitespace.
 *
 * @param s Type text.
 *
 * @return New string.
 */
char *strip_const_word(const char *s)
{
	/* Hoisted "const int x = 5" becomes "int x;" plus "x = 5;".
	 * Dropping const keeps that assignment legal; nothing in valid
	 * input modifies the variable anyway, so runtime behaviour is
	 * unchanged. */
	StrBuf b;

	sb_init(&b);

	/* tokenize by spaces? simpler: scan for word const with boundaries */
	size_t n = strlen(s), i = 0;
	int first = 1;

	while (i < n) {
		if (!strncmp(s + i, "const", 5) &&
		    (i == 0 || !is_ident_char(s[i - 1])) &&
		    (i + 5 >= n || !is_ident_char(s[i + 5]))) {
			/* Whole word "const": skip it.  A mere substring
			 * such as "constant" falls through below. */
			i += 5;
			continue;
		}

		if (isspace((unsigned char)s[i])) {
			if (!first && b.len && b.data[b.len - 1] != ' ')
				sb_putc(&b, ' ');
			while (i < n && isspace((unsigned char)s[i]))
				i++;
			continue;
		}

		first = 0;
		sb_putc(&b, s[i]);
		i++;
	}

	/* trim */
	while (b.len && b.data[b.len - 1] == ' ')
		b.data[--b.len] = '\0';
	if (b.len == 0) {
		/* Type was bare "const": keep the original text rather
		 * than emitting an empty type. */
		free(b.data);
		return xstrdup(s);
	}

	return b.data;
}

/**
 * @brief Test for struct/union types.
 *
 * @param t Type text.
 *
 * @return 1 for aggregates.
 */
int is_struct_type(const char *t)
{
	return contains_word(t, "struct") || contains_word(t, "union");
}

/**
 * @brief Measure a string literal with NUL in bytes.
 *
 * @param lit Literal text.
 *
 * @return Size; escapes count once.
 */
int c_str_lit_size(const char *lit)
{
	/* Only the first literal is measured; adjacent "" pairs are
	 * rare enough to ignore for a size hint. */
	const char *p = strchr(lit, '"');

	if (!p)
		return 64;
	p++;
	int cnt = 0;

	while (*p && *p != '"') {
		if (*p == '\\') {
			p++;
			if (*p == 'x' || *p == 'X') {
				/* Hex escape: one byte however long. */
				p++;
				while (isxdigit((unsigned char)*p))
					p++;
				cnt++;
			} else if (*p >= '0' && *p <= '7') {
				/* Octal escape: at most three digits. */
				int k = 0;

				while (k < 3 && *p >= '0' && *p <= '7') {
					p++;
					k++;
				}

				cnt++;
			} else if (*p) {
				/* Any other escape is one character. */
				p++;
				cnt++;
			} else

				break;
		} else {
			p++;
			cnt++;
		}
	}

	/* Plus the terminating NUL. */
	return cnt + 1;
}

/**
 * @brief Size the first '[]' pair.
 *
 * @param suffix Declarator suffix.
 * @param size Deduced size.
 *
 * @return New string.
 */
char *replace_first_empty_brackets(const char *suffix, int size)
{
	const char *p = suffix;
	const char *found = NULL;

	/* Find the first "[]" pair, tolerating spaces as in "[ ]". */
	while (*p) {
		if (*p == '[') {
			const char *q = p + 1;

			while (*q == ' ' || *q == '\t')
				q++;
			if (*q == ']') {
				found = p;
				break;
			}
		}

		p++;
	}

	if (!found)
		return xstrdup(suffix);
	const char *q = found + 1;

	while (*q == ' ' || *q == '\t')
		q++;
	if (*q == ']')
		q++;
	StrBuf b;

	/* Splice "[size]" in place of the empty pair. */
	sb_init(&b);
	sb_putn(&b, suffix, found - suffix);
	char tmp[64];

	snprintf(tmp, sizeof(tmp), "[%d]", size);
	sb_puts(&b, tmp);
	sb_puts(&b, q);
	return b.data;
}

/**
 * @brief Deduce '[]' from string or '{...}' initializers.
 *
 * @param suffix Declarator suffix.
 * @param init_text Initializer, or NULL.
 *
 * @return New suffix string.
 */
char *fix_array_suffix(const char *suffix, const char *init_text)
{
	char *s = xstrdup(suffix ? suffix : "");

	/* Nothing to do unless an unsized "[]" is present. */
	{
		int has = 0;

		for (const char *p = s; *p; p++)
			if (*p == '[') {
				const char *q = p + 1;

				while (*q == ' ' || *q == '\t')
					q++;
				if (*q == ']') {
					has = 1;
					break;
				}
			}

		if (!has)
			return s;
	}

	if (!init_text)
		return s;
	while (*init_text && isspace((unsigned char)*init_text))
		init_text++;
	size_t L = strlen(init_text);

	while (L && isspace((unsigned char)init_text[L - 1]))
		L--;
	char *it = xstrndup(init_text, L);
	char *res = NULL;

	if ((it[0] == '"') || (it[0] == '\'')) {
		/* "char s[] = "hi"": size is string length plus NUL. */
		int sz = c_str_lit_size(it);

		res = replace_first_empty_brackets(s, sz);
	} else if (it[0] == '{') {
		/* "int a[] = {...}": elements are top-level commas plus
		 * one; nested braces don't contribute. */

		/* strip outer braces: find inner */
		size_t il = strlen(it);
		const char *inner = it + 1;
		size_t inlen = (il >= 2 && it[il - 1] == '}') ? il - 2 : il - 1;
		char *inner_s = xstrndup(inner, inlen);

		/* An empty list means a zero-length array. */
		int blank = 1;

		for (char *q = inner_s; *q; q++)
			if (!isspace((unsigned char)*q)) {
				blank = 0;
				break;
			}

		int nelem = 0;

		if (!blank) {
			TokVec tv = tokenize(inner_s);
			int d = 0, commas = 0;

			for (size_t i = 0; i < tv.len; i++) {
				const char *tx = tv.items[i].text;

				if (streq(tx, "(") || streq(tx, "[") ||
				    streq(tx, "{"))
					d++;
				else if (streq(tx, ")") || streq(tx, "]") ||
					 streq(tx, "}"))
					d--;
				else if (streq(tx, ",") && d == 0)
					commas++;
			}

			tv_free(&tv);
			nelem = commas + 1;
		}

		free(inner_s);
		res = replace_first_empty_brackets(s, nelem);
	} else {
		res = s;
		s = NULL;
	}

	free(it);
	if (s)
		free(s);
	return res ? res : xstrdup(suffix);
}

/**
 * @brief Parse a declaration fragment such as for-init.
 *
 * @param h Hoister.
 * @param s Fragment text.
 *
 * @return New declaration node.
 */
Node *parse_decl_string(Hoister *h, const char *s)
{
	StrBuf b;

	sb_init(&b);
	sb_puts(&b, s);
	sb_puts(&b, " ;");
	TokVec tv = tokenize(b.data);

	free(b.data);
	Parser p;

	/* Reuse the statement parser by faking a one-declaration input.
	 * The typedef set is shared so new names stay visible. */
	p.toks = tv.items;
	p.n = tv.len;
	p.pos = 0;
	p.typedefs = h->typedefs;
	Node *n = parse_declaration(&p);

	/* parse_declaration copies what it keeps via toks_to_str, so the
	 * token vector itself can go. */
	tv_free(&tv);
	return n;
}

/**
 * @brief Lower an array initializer to runtime copies.
 *
 * @param h Hoister.
 * @param new_name Hoisted array.
 * @param init_text Initializer text.
 *
 * @return One or two expression nodes.
 */
NodeVec make_array_init(Hoister *h, const char *new_name, const char *init_text)
{
	NodeVec v;

	nv_init(&v);
	while (*init_text && isspace((unsigned char)*init_text))
		init_text++;
	if (init_text[0] == '"' || init_text[0] == '\'') {
		/* String into char array: clear first, then copy at most
		 * what fits.  __builtin_ variants need no extra #include. */
		StrBuf a;

		sb_init(&a);
		sb_puts(&a, "__builtin_memset ( ");
		sb_puts(&a, new_name);
		sb_puts(&a, " , 0 , sizeof ( ");
		sb_puts(&a, new_name);
		sb_puts(&a, " ) ) ;");
		Node *n1 = node_new(N_EXPR);

		n1->expr_text = a.data;
		nv_push(&v, n1);
		StrBuf b2;

		sb_init(&b2);
		sb_puts(&b2, "__builtin_memcpy ( ");
		sb_puts(&b2, new_name);
		sb_puts(&b2, " , ");
		sb_puts(&b2, init_text);
		sb_puts(&b2, " , sizeof ( ");
		sb_puts(&b2, init_text);
		sb_puts(&b2, " ) < sizeof ( ");
		sb_puts(&b2, new_name);
		sb_puts(&b2, " ) ? sizeof ( ");
		sb_puts(&b2, init_text);
		sb_puts(&b2, " ) : sizeof ( ");
		sb_puts(&b2, new_name);
		sb_puts(&b2, " ) ) ;");
		Node *n2 = node_new(N_EXPR);

		n2->expr_text = b2.data;
		nv_push(&v, n2);
	} else if (init_text[0] == '{') {
		h->temp_counter++;
		char tmp[64];

		snprintf(tmp, sizeof(tmp), "__flat_init_%d", h->temp_counter);
		StrBuf c;

		/* Brace list into an array: stage it through a static const
		 * helper of matching element type, zero the target, then
		 * copy the smaller of the two sizes. */
		sb_init(&c);
		sb_puts(&c, "{ static const __typeof__ ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " [0]) ");
		sb_puts(&c, tmp);
		sb_puts(&c, " [] = ");
		sb_puts(&c, init_text);
		sb_puts(&c, " ; ");
		sb_puts(&c, "__builtin_memset ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " , 0 , sizeof ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " ) ) ; ");
		sb_puts(&c, "__builtin_memcpy ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " , ");
		sb_puts(&c, tmp);
		sb_puts(&c, " , sizeof ( ");
		sb_puts(&c, tmp);
		sb_puts(&c, " ) < sizeof ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " ) ? sizeof ( ");
		sb_puts(&c, tmp);
		sb_puts(&c, " ) : sizeof ( ");
		sb_puts(&c, new_name);
		sb_puts(&c, " ) ) ; }");
		Node *n = node_new(N_EXPR);

		n->expr_text = c.data;
		nv_push(&v, n);
	} else {
		/* Anything else (invalid C, but be graceful): at least zero
		 * the array. */
		StrBuf a;

		sb_init(&a);
		sb_puts(&a, "__builtin_memset ( ");
		sb_puts(&a, new_name);
		sb_puts(&a, " , 0 , sizeof ( ");
		sb_puts(&a, new_name);
		sb_puts(&a, " ) ) ;");
		Node *n1 = node_new(N_EXPR);

		n1->expr_text = a.data;
		nv_push(&v, n1);
	}

	return v;
}

/**
 * @brief Append every node of one vector to another.
 *
 * @param dst Destination.
 * @param src Source (kept).
 */
void nv_extend(NodeVec *dst, NodeVec *src)
{
	for (size_t i = 0; i < src->len; i++)
		nv_push(dst, src->items[i]);
}

/**
 * @brief Hoist declarations and rewrite identifiers.
 *
 * @param h Hoister.
 * @param nd Node to transform.
 *
 * @return Replacement nodes; declarations may expand.
 */
NodeVec hoist_node(Hoister *h, Node *nd)
{
	NodeVec out;

	nv_init(&out);
	if (!nd)
		return out;
	int tp = nd->type;

	if (tp == N_BLOCK) {
		h_push(h);
		Node *nb = node_new(N_BLOCK);

		nv_init(&nb->stmts);
		/* Hoist each member; one input statement can expand to
		 * several (e.g. a multi-declarator "int a = 1, b = 2;"). */
		for (size_t i = 0; i < nd->stmts.len; i++) {
			NodeVec r = hoist_node(h, nd->stmts.items[i]);

			nv_extend(&nb->stmts, &r);
			free(r.items);
		}

		h_pop(h);
		nv_push(&out, nb);
	} else if (tp == N_DECL) {
		NodeVec stmts;

		nv_init(&stmts);
		for (size_t i = 0; i < nd->decls.len; i++) {
			DeclEnt *d = &nd->decls.items[i];

			/* A nameless declarator is an inner function
			 * prototype: no runtime effect, hoist verbatim. */
			if (!d->name) { /* func decl */
				StrBuf b;

				sb_init(&b);
				sb_puts(&b, d->raw);
				sb_puts(&b, " ;");
				sv_push(&h->hoisted, b.data);
				continue;
			}

			char *init_rw =
				d->init ? rewrite_expr(h, d->init) : NULL;
			char *fixed = fix_array_suffix(
				d->suffix ? d->suffix : "", init_rw);
			int is_static = contains_word(nd->type_str, "static");

			/* Static locals initialise once at program start, so
			 * the initializer stays in the hoisted declaration
			 * and nothing runs in place. */
			if (is_static) {
				char *nn = h_declare(h, d->name, nd->type_str,
						     d->stars ? d->stars : "",
						     fixed, init_rw);
				free(nn);
				free(fixed);
				if (init_rw)
					free(init_rw);
				continue;
			}

			char *ht = strip_const_word(nd->type_str);

			char *nn = h_declare(h, d->name, ht,
					     d->stars ? d->stars : "", fixed,
					     NULL);

			free(ht);
			free(fixed);
			if (init_rw) {
				int is_arr =
					d->suffix && strchr(d->suffix, '[');

				if (is_arr) {
					NodeVec ai =
						make_array_init(h, nn, init_rw);
					nv_extend(&stmts, &ai);
					free(ai.items);
				} else if (is_struct_type(nd->type_str)) {
					StrBuf b;

					sb_init(&b);

					const char *it = init_rw;

					while (*it &&
					       isspace((unsigned char)*it))
						it++;
					if (it[0] == '{') {
						/* Brace list: assign through a
						 * compound literal of the
						 * declared type. */
						sb_puts(&b, nn);
						sb_puts(&b, " = (");
						sb_puts(&b, nd->type_str);
						sb_puts(&b, ") ");
						sb_puts(&b, init_rw);
						sb_puts(&b, " ;");
					} else {
						/* Copy from another value, e.g.
						 * "struct S s = other;". */
						sb_puts(&b, nn);
						sb_puts(&b, " = ");
						sb_puts(&b, init_rw);
						sb_puts(&b, " ;");
					}

					Node *en = node_new(N_EXPR);

					en->expr_text = b.data;
					nv_push(&stmts, en);
				} else {
					char *it = xstrdup(init_rw);

					char *s = it;

					while (*s && isspace((unsigned char)*s))
						s++;
					size_t L = strlen(s);

					while (L &&
					       isspace((unsigned char)s[L - 1]))
						s[--L] = '\0';
					char *use = s;
					char *unwrapped = NULL;

					/* C allows "int x = {5};": unwrap a
					 * lone braced value to a plain
					 * assignment. */
					if (s[0] == '{' && L >= 2 &&
					    s[L - 1] == '}') {
						char *inner =
							xstrndup(s + 1, L - 2);

						char *q = inner;

						while (*q &&
						       isspace((
							       unsigned char)*q))
							q++;
						size_t L2 = strlen(q);

						while (L2 &&
						       isspace((unsigned char)
								       q[L2 -
									 1]))
							q[--L2] = '\0';
						if (!strchr(q, ',')) {
							unwrapped = xstrdup(q);
							use = unwrapped;
						}

						free(inner);
					}

					StrBuf b;

					sb_init(&b);
					sb_puts(&b, nn);
					sb_puts(&b, " = ");
					sb_puts(&b, use);
					sb_puts(&b, " ;");
					if (unwrapped)
						free(unwrapped);
					free(it);
					Node *en = node_new(N_EXPR);

					en->expr_text = b.data;
					nv_push(&stmts, en);
				}

				free(init_rw);
				free(nn);
			} else

				free(nn);
		}

		nv_extend(&out, &stmts);
		free(stmts.items);
	} else if (tp == N_DECL_NOVAR) {
		sv_push(&h->hoisted, xstrdup(nd->decl_text));
	} else if (tp == N_TYPEDEF) {
		sv_push(&h->hoisted, xstrdup(nd->decl_text));

		/* Remember the fresh type name for later declarations. */
		TokVec tv = tokenize(nd->decl_text);
		const char *last = NULL;

		for (size_t i = 0; i < tv.len; i++)
			if (tv.items[i].kind == TOK_IDENT)
				last = tv.items[i].text;
		if (last && !sv_contains(h->typedefs, last))
			sv_push(h->typedefs, xstrdup(last));
		tv_free(&tv);
	} else if (tp == N_EXPR) {
		char *txt = xstrdup(nd->expr_text);

		size_t L = strlen(txt);

		while (L && isspace((unsigned char)txt[L - 1]))
			txt[--L] = '\0';
		if (L && txt[L - 1] == ';') {
			txt[--L] = '\0';
			while (L && isspace((unsigned char)txt[L - 1]))
				txt[--L] = '\0';
		}

		char *rw = rewrite_expr(h, txt);

		free(txt);
		StrBuf b;

		sb_init(&b);
		sb_puts(&b, rw);
		sb_puts(&b, " ;");
		free(rw);
		Node *n = node_new(N_EXPR);

		n->expr_text = b.data;
		nv_push(&out, n);
	} else if (tp == N_IF) {
		char *nc = rewrite_expr(h, nd->cond);

		/* Each branch gets a scope; a block pushes its own again,
		 * which is harmless nesting for straight-line branches. */
		h_push(h);
		NodeVec th = hoist_node(h, nd->then_b);

		h_pop(h);
		h_push(h);
		NodeVec el;

		nv_init(&el);
		if (nd->else_b) {
			el = hoist_node(h, nd->else_b);
		}

		h_pop(h);
		Node *thn = NULL, *eln = NULL;

		/* Splice single results straight in; wrap longer ones so
		 * the branch stays one node. */
		if (th.len == 1)
			thn = th.items[0];
		else if (th.len > 1) {
			thn = node_new(N_BLOCK);
			thn->stmts = th;
		} else {
			thn = node_new(N_BLOCK);
			nv_init(&thn->stmts);
			free(th.items);
		}

		if (nd->else_b) {
			if (el.len == 1)
				eln = el.items[0];
			else if (el.len > 1) {
				eln = node_new(N_BLOCK);
				eln->stmts = el;
			} else {
				eln = node_new(N_BLOCK);
				nv_init(&eln->stmts);
				free(el.items);
			}
		} else {
			free(el.items);
		}

		if (th.len > 1) {
			/* Array moved into the block; nothing to free. */
		} else

			free(th.items);
		Node *n = node_new(N_IF);

		n->cond = nc;
		n->then_b = thn;
		n->else_b = eln;
		nv_push(&out, n);
	} else if (tp == N_WHILE) {
		char *nc = rewrite_expr(h, nd->wcond);

		h_push(h);
		NodeVec b = hoist_node(h, nd->body);

		h_pop(h);
		Node *bn = NULL;

		if (b.len == 1)
			bn = b.items[0];
		else if (b.len > 1) {
			bn = node_new(N_BLOCK);
			bn->stmts = b;
		} else {
			bn = node_new(N_BLOCK);
			nv_init(&bn->stmts);
			free(b.items);
		}

		if (b.len > 1) {
			/* Array moved into the block; nothing to free. */
		} else

			free(b.items);
		Node *n = node_new(N_WHILE);

		n->wcond = nc;
		n->body = bn;
		nv_push(&out, n);
	} else if (tp == N_DOWHILE) {
		h_push(h);
		NodeVec b = hoist_node(h, nd->body);

		h_pop(h);
		Node *bn = NULL;

		if (b.len == 1)
			bn = b.items[0];
		else if (b.len > 1) {
			bn = node_new(N_BLOCK);
			bn->stmts = b;
		} else {
			bn = node_new(N_BLOCK);
			nv_init(&bn->stmts);
			free(b.items);
		}

		if (b.len > 1) {
			/* Array moved into the block; nothing to free. */
		} else

			free(b.items);
		char *nc = rewrite_expr(h, nd->wcond);
		Node *n = node_new(N_DOWHILE);

		n->body = bn;
		n->wcond = nc;
		nv_push(&out, n);
	} else if (tp == N_FOR) {
		h_push(h);
		NodeVec pre;

		nv_init(&pre);
		char *init_expr_rw = NULL, *cond_rw = NULL, *incr_rw = NULL;

		/* A "for (int i = ...)" init declares variables: hoist them
		 * now and splice the resulting assignments ahead of the
		 * loop itself. */
		if (nd->init_decl) {
			Node *dn = parse_decl_string(h, nd->init_decl);

			if (dn) {
				NodeVec r = hoist_node(h, dn);

				nv_extend(&pre, &r);
				free(r.items);
			}
		}

		if (nd->init_expr)
			init_expr_rw = rewrite_expr(h, nd->init_expr);
		if (nd->for_cond) {
			const char *p = nd->for_cond;

			while (*p && isspace((unsigned char)*p))
				p++;
			/* An empty condition means "always true". */
			if (*p == '\0')
				cond_rw = xstrdup("");
			else
				cond_rw = rewrite_expr(h, nd->for_cond);
		} else

			cond_rw = xstrdup("");
		if (nd->incr) {
			const char *p = nd->incr;

			while (*p && isspace((unsigned char)*p))
				p++;
			if (*p == '\0')
				incr_rw = xstrdup("");
			else
				incr_rw = rewrite_expr(h, nd->incr);
		} else

			incr_rw = xstrdup("");
		NodeVec b = hoist_node(h, nd->for_body);
		Node *bn = NULL;

		if (b.len == 1)
			bn = b.items[0];
		else if (b.len > 1) {
			bn = node_new(N_BLOCK);
			bn->stmts = b;
		} else {
			bn = node_new(N_BLOCK);
			nv_init(&bn->stmts);
			free(b.items);
		}

		if (b.len > 1) {
			/* Array moved into the block; nothing to free. */
		} else

			free(b.items);
		h_pop(h);
		Node *n = node_new(N_FOR);

		n->init_decl = NULL;
		n->init_expr = init_expr_rw;
		n->for_cond = cond_rw;
		n->incr = incr_rw;
		n->for_body = bn;
		/* Initializer assignments run before the loop node. */
		nv_extend(&out, &pre);
		free(pre.items);
		nv_push(&out, n);
	} else if (tp == N_SWITCH) {
		char *ne = rewrite_expr(h, nd->sw_expr);

		h_push(h);
		NodeVec b = hoist_node(h, nd->sw_body);

		h_pop(h);
		Node *bn = NULL;

		if (b.len == 1)
			bn = b.items[0];
		else if (b.len > 1) {
			bn = node_new(N_BLOCK);
			bn->stmts = b;
		} else {
			bn = node_new(N_BLOCK);
			nv_init(&bn->stmts);
			free(b.items);
		}

		if (b.len > 1) {
			/* Array moved into the block; nothing to free. */
		} else

			free(b.items);
		Node *n = node_new(N_SWITCH);

		n->sw_expr = ne;
		n->sw_body = bn;
		nv_push(&out, n);
	} else if (tp == N_CASE) {
		char *ne = rewrite_expr(h, nd->case_expr);
		Node *n = node_new(N_CASE);

		n->case_expr = ne;
		nv_push(&out, n);
	} else if (tp == N_DEFAULT || tp == N_GOTO || tp == N_LABEL ||

		   tp == N_BREAK || tp == N_CONTINUE || tp == N_EMPTY) {
		nv_push(&out, nd);
	} else if (tp == N_LABELED) {
		NodeVec r = hoist_node(h, nd->labeled_stmt);

		if (r.len > 1) {
			/* A label plus several statements: split into a bare
			 * label node followed by a block holding the rest. */
			Node *lb = node_new(N_LABEL);

			lb->label = xstrdup(nd->label);
			nv_push(&out, lb);
			Node *blk = node_new(N_BLOCK);

			blk->stmts = r;
			nv_push(&out, blk);
		} else if (r.len == 1) {
			Node *n = node_new(N_LABELED);

			n->label = xstrdup(nd->label);
			n->labeled_stmt = r.items[0];
			nv_push(&out, n);
			free(r.items);
		} else {
			Node *n = node_new(N_LABEL);

			n->label = xstrdup(nd->label);
			nv_push(&out, n);
			free(r.items);
		}
	} else if (tp == N_RETURN) {
		Node *n = node_new(N_RETURN);

		n->ret_expr =
			nd->ret_expr ? rewrite_expr(h, nd->ret_expr) : NULL;
		nv_push(&out, n);
	} else {
		nv_push(&out, nd);
	}

	return out;
}
