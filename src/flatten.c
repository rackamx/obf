/**
 * @file flatten.c
 * @brief Control-flow flattening: hoisting, lowering, emission.
 */
#include "flatten.h"
#include "parser.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ---------------- hoister ---------------- */
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
	/* push base scope */
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
	/* remove all occurrences of word const */
	StrBuf b;

	sb_init(&b);
	/* tokenize by spaces? simpler: scan for word const with boundaries */
	size_t n = strlen(s), i = 0;
	int first = 1;

	while (i < n) {
		if (!strncmp(s + i, "const", 5) &&
		    (i == 0 || !is_ident_char(s[i - 1])) &&
		    (i + 5 >= n || !is_ident_char(s[i + 5]))) {
			i += 5;
			continue;
		}

		/* copy one char? better copy token-wise: copy char */
		/* collapse multiple spaces */
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

/* c string literal length (bytes) +1 for NUL; input like "\"hi\"" */
/**
 * @brief Measure a string literal with NUL in bytes.
 *
 * @param lit Literal text.
 *
 * @return Size; escapes count once.
 */
int c_str_lit_size(const char *lit)
{
	/* find first " ... " */
	const char *p = strchr(lit, '"');

	if (!p)
		return 64;
	p++;
	int cnt = 0;

	while (*p && *p != '"') {
		if (*p == '\\') {
			p++;
			if (*p == 'x' || *p == 'X') {
				p++;
				while (isxdigit((unsigned char)*p))
					p++;
				cnt++;
			} else if (*p >= '0' && *p <= '7') {
				int k = 0;

				while (k < 3 && *p >= '0' && *p <= '7') {
					p++;
					k++;
				}

				cnt++;
			} else if (*p) {
				p++;
				cnt++;
			} else

				break;
		} else {
			p++;
			cnt++;
		}
	}

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

	/* trim */
	/* check contains [] ignoring spaces */
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
		int sz = c_str_lit_size(it);

		res = replace_first_empty_brackets(s, sz);
	} else if (it[0] == '{') {
		/* count top-level commas via tokenize */
		/* strip outer braces: find inner */
		size_t il = strlen(it);
		const char *inner = it + 1;
		size_t inlen = (il >= 2 && it[il - 1] == '}') ? il - 2 : il - 1;
		char *inner_s = xstrndup(inner, inlen);

		/* check blank */
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

/* hoist: returns NodeVec (owned Nodes). Frees/transforms input? We transform in
 * place and produce list. */
NodeVec hoist_node(Hoister *h, Node *nd);

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

	p.toks = tv.items;
	p.n = tv.len;
	p.pos = 0;
	p.typedefs = h->typedefs;
	Node *n = parse_declaration(&p);

	/* leak tv texts? parse_declaration copied needed strings via
	 * toks_to_str (which copies). Free tv. */
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
				/* note: use fixed suffix? recompute: if suffix
				 * had '[' then array */
				/* we freed fixed; need to know if array: check
				 * original suffix */
				if (is_arr) {
					NodeVec ai =
						make_array_init(h, nn, init_rw);
					nv_extend(&stmts, &ai);
					free(ai.items);
				} else if (is_struct_type(nd->type_str)) {
					StrBuf b;

					sb_init(&b);
					/* trim init */
					const char *it = init_rw;

					while (*it &&
					       isspace((unsigned char)*it))
						it++;
					if (it[0] == '{') {
						sb_puts(&b, nn);
						sb_puts(&b, " = (");
						sb_puts(&b, nd->type_str);
						sb_puts(&b, ") ");
						sb_puts(&b, init_rw);
						sb_puts(&b, " ;");
					} else {
						sb_puts(&b, nn);
						sb_puts(&b, " = ");
						sb_puts(&b, init_rw);
						sb_puts(&b, " ;");
					}

					Node *en = node_new(N_EXPR);

					en->expr_text = b.data;
					nv_push(&stmts, en);
				} else {
					/* scalar, strip single braces {val} */
					char *it = xstrdup(init_rw);

					/* trim */
					char *s = it;

					while (*s && isspace((unsigned char)*s))
						s++;
					size_t L = strlen(s);

					while (L &&
					       isspace((unsigned char)s[L - 1]))
						s[--L] = '\0';
					char *use = s;
					char *unwrapped = NULL;

					if (s[0] == '{' && L >= 2 &&
					    s[L - 1] == '}') {
						char *inner =
							xstrndup(s + 1, L - 2);
						/* trim inner */
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
		/* record name */
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

		/* strip trailing ; */
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

		if (th.len > 1) { /* th moved */
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
			/* trim */
			const char *p = nd->for_cond;

			while (*p && isspace((unsigned char)*p))
				p++;
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
		} else

			free(b.items);
		h_pop(h);
		Node *n = node_new(N_FOR);

		n->init_decl = NULL;
		n->init_expr = init_expr_rw;
		n->for_cond = cond_rw;
		n->incr = incr_rw;
		n->for_body = bn;
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
			Node *lb = node_new(N_LABEL);

			lb->label = xstrdup(nd->label);
			nv_push(&out, lb);
			Node *blk = node_new(N_BLOCK);

			blk->stmts = r;
			nv_push(&out, blk);
			/* return two nodes: need to push both; out already has?
			 * we pushed lb+blk, done */
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

/* ---------------- lowerer ---------------- */
/**
 * @brief Basic block terminators.
 */
enum {
	TERM_NONE = 0, /**< Open block. */
	TERM_GOTO,     /**< Unconditional jump. */
	TERM_COND,     /**< Conditional branch. */
	TERM_RETURN,   /**< Function return. */
	TERM_EXIT      /**< Fall off the end. */
};

/**
 * @brief Basic block.
 */
typedef struct {
	StrVec stmts;	  /**< Straight-line statements. */
	int term;	  /**< Terminator (TERM_*). */
	int target;	  /**< Jump or true target. */
	int target2;	  /**< False target. */
	char *cond;	  /**< Branch condition. */
	char *ret_expr;	  /**< Return value, or NULL. */
	char *goto_label; /**< Pending label, resolved later. */
} Block;

/**
 * @brief Basic block vector.
 */
typedef struct {
	Block *items; /**< Blocks. */
	size_t len;   /**< Item count. */
	size_t cap;   /**< Allocated slots. */
} BlockVec;

/**
 * @brief User label binding.
 */
typedef struct {
	char *name; /**< Label name. */
	int bid;    /**< Target block. */
} LabelEnt;

/**
 * @brief Label binding vector.
 */
typedef struct {
	LabelEnt *items; /**< Entries. */
	size_t len;	 /**< Item count. */
	size_t cap;	 /**< Allocated slots. */
} LabelVec;

/**
 * @brief Break/continue targets.
 */
typedef struct {
	int has_break; /**< Break is valid. */
	int brk;       /**< Break target. */
	int has_cont;  /**< Continue is valid. */
	int cont;      /**< Continue target. */
} LoopCtx;

/**
 * @brief AST-to-blocks lowerer.
 */
typedef struct {
	Hoister *ho;	 /**< Hoister (borrowed). */
	BlockVec blocks; /**< Basic blocks. */
	int cur;	 /**< Current block. */
	int entry;	 /**< Entry block. */
	LabelVec labels; /**< User labels. */
	LoopCtx *loops;	 /**< Loop contexts. */
	size_t llen;	 /**< Loop depth. */
	size_t lcap;	 /**< Loop capacity. */
} Lowerer;

/**
 * @brief Initialise a lowerer bound to a hoister.
 *
 * @param L Lowerer.
 * @param h Hoister.
 */
void lower_init(Lowerer *L, Hoister *h)
{
	L->ho = h;
	L->blocks.items = NULL;
	L->blocks.len = L->blocks.cap = 0;
	L->cur = -1;
	L->entry = -1;
	L->labels.items = NULL;
	L->labels.len = L->labels.cap = 0;
	L->loops = NULL;
	L->llen = 0;
	L->lcap = 0;
}

/**
 * @brief Create an empty basic block.
 *
 * @param L Lowerer.
 *
 * @return New block id.
 */
int lower_new_block(Lowerer *L)
{
	if (L->blocks.len == L->blocks.cap) {
		size_t nc = L->blocks.cap ? L->blocks.cap * 2 : 16;

		L->blocks.items =
			(Block *)xrealloc(L->blocks.items, nc * sizeof(Block));
		L->blocks.cap = nc;
	}

	Block *b = &L->blocks.items[L->blocks.len];

	sv_init(&b->stmts);
	b->term = TERM_NONE;
	b->target = b->target2 = -1;
	b->cond = NULL;
	b->ret_expr = NULL;
	b->goto_label = NULL;
	return (int)L->blocks.len++;
}

/**
 * @brief Append straight-line code to the current block.
 *
 * @param L Lowerer.
 * @param code Statement text.
 */
void lower_emit(Lowerer *L, const char *code)
{
	if (!code)
		return;
	while (*code && isspace((unsigned char)*code))
		code++;
	if (!*code)
		return;
	/* trim trailing spaces */
	size_t n = strlen(code);

	while (n && isspace((unsigned char)code[n - 1]))
		n--;
	char *s = xstrndup(code, n);

	/* ensure ends with ; or } */
	size_t L2 = strlen(s);
	int ends = (L2 > 0 && (s[L2 - 1] == ';' || s[L2 - 1] == '}'));
	Block *b = &L->blocks.items[L->cur];

	if (!ends) {
		StrBuf bb;

		sb_init(&bb);
		sb_puts(&bb, s);
		sb_puts(&bb, " ;");
		free(s);
		sv_push(&b->stmts, bb.data);
	} else

		sv_push(&b->stmts, s);
}

/**
 * @brief Test whether the current block is terminated.
 *
 * @param L Lowerer.
 *
 * @return Non-zero when terminated.
 */
int lower_cur_term(Lowerer *L)
{
	return L->blocks.items[L->cur].term != TERM_NONE;
}

/**
 * @brief Push break/continue targets for a loop or switch.
 *
 * @param L Lowerer.
 * @param hb Break valid.
 * @param b Break target.
 * @param hc Continue valid.
 * @param c Continue target.
 */
void loop_push(Lowerer *L, int hb, int b, int hc, int c)
{
	if (L->llen == L->lcap) {
		size_t nc = L->lcap ? L->lcap * 2 : 8;
		L->loops = (LoopCtx *)xrealloc(L->loops, nc * sizeof(LoopCtx));
		L->lcap = nc;
	}

	L->loops[L->llen].has_break = hb;
	L->loops[L->llen].brk = b;
	L->loops[L->llen].has_cont = hc;
	L->loops[L->llen].cont = c;
	L->llen++;
}

/**
 * @brief Pop the innermost loop context.
 *
 * @param L Lowerer.
 */
void loop_pop(Lowerer *L)
{
	if (L->llen)
		L->llen--;
}

/**
 * @brief Resolve a user label to its block.
 *
 * @param L Lowerer.
 * @param n Label name.
 *
 * @return Block id, or -1 when unknown.
 */
int find_label(Lowerer *L, const char *n)
{
	for (size_t i = 0; i < L->labels.len; i++)
		if (streq(L->labels.items[i].name, n))
			return L->labels.items[i].bid;
	return -1;
}

/**
 * @brief Bind a user label to a block.
 *
 * @param L Lowerer.
 * @param n Label name.
 * @param bid Block id.
 */
void set_label(Lowerer *L, const char *n, int bid)
{
	for (size_t i = 0; i < L->labels.len; i++)
		if (streq(L->labels.items[i].name, n)) {
			L->labels.items[i].bid = bid;

			return;
		}

	if (L->labels.len == L->labels.cap) {
		size_t nc = L->labels.cap ? L->labels.cap * 2 : 8;

		L->labels.items = (LabelEnt *)xrealloc(L->labels.items,
						       nc * sizeof(LabelEnt));
		L->labels.cap = nc;
	}

	L->labels.items[L->labels.len].name = xstrdup(n);
	L->labels.items[L->labels.len].bid = bid;
	L->labels.len++;
}

void lower_stmt(Lowerer *L, Node *nd);

/**
 * @brief Switch case target.
 */
typedef struct {
	char *expr; /**< Case value (NULL for default). */
	int is_def; /**< Non-zero for default. */
	int blk;    /**< Target block. */
} CaseEnt;

/**
 * @brief Switch case vector.
 */
typedef struct {
	CaseEnt *items; /**< Entries. */
	size_t len;	/**< Item count. */
	size_t cap;	/**< Allocated slots. */
} CaseVec;

/**
 * @brief Record a switch case target.
 *
 * @param v Vector.
 * @param e Case value (NULL for default).
 * @param isd Non-zero for default.
 * @param b Block id.
 */
void casevec_push(CaseVec *v, char *e, int isd, int b)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 8;

		v->items = (CaseEnt *)xrealloc(v->items, nc * sizeof(CaseEnt));
		v->cap = nc;
	}

	v->items[v->len].expr = e;
	v->items[v->len].is_def = isd;
	v->items[v->len].blk = b;
	v->len++;
}

/**
 * @brief Lower a switch body, recording case targets.
 *
 * @param L Lowerer.
 * @param nd Body node.
 * @param cases Targets.
 */
void sw_walk(Lowerer *L, Node *nd, CaseVec *cases)
{
	if (!nd)
		return;
	int tp = nd->type;

	if (tp == N_BLOCK) {
		for (size_t i = 0; i < nd->stmts.len; i++)
			sw_walk(L, nd->stmts.items[i], cases);
	} else if (tp == N_CASE) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		casevec_push(cases, xstrdup(nd->case_expr), 0, nb);
	} else if (tp == N_DEFAULT) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		casevec_push(cases, NULL, 1, nb);
	} else if (tp == N_LABEL) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		set_label(L, nd->label, nb);
	} else if (tp == N_LABELED) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		set_label(L, nd->label, nb);
		sw_walk(L, nd->labeled_stmt, cases);
	} else {
		lower_stmt(L, nd);
	}
}

/**
 * @brief Lower a switch with an equality dispatch chain.
 *
 * @param L Lowerer.
 * @param nd Switch node.
 */
void lower_switch(Lowerer *L, Node *nd)
{
	L->ho->temp_counter++;
	char tmp[64];

	snprintf(tmp, sizeof(tmp), "__flat_sw_%d", L->ho->temp_counter);
	StrBuf d;

	sb_init(&d);
	sb_puts(&d, "__typeof__ ((");
	sb_puts(&d, nd->sw_expr);
	sb_puts(&d, ")) ");
	sb_puts(&d, tmp);
	sb_puts(&d, " ;");
	sv_push(&L->ho->hoisted, d.data);
	StrBuf as;

	sb_init(&as);
	sb_puts(&as, tmp);
	sb_puts(&as, " = (");
	sb_puts(&as, nd->sw_expr);
	sb_puts(&as, ") ;");
	lower_emit(L, as.data);
	free(as.data);
	int end_b = lower_new_block(L), disp_b = lower_new_block(L);
	L->blocks.items[L->cur].term = TERM_GOTO;
	L->blocks.items[L->cur].target = disp_b;
	int start_b = lower_new_block(L);
	L->cur = start_b;
	CaseVec cases;

	cases.items = NULL;
	cases.len = 0;
	cases.cap = 0;
	loop_push(L, 1, end_b, 0, 0);
	sw_walk(L, nd->sw_body, &cases);
	if (!lower_cur_term(L)) {
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = end_b;
		L->cur = lower_new_block(L);
	}

	/* build dispatch chain */
	int def_tgt = end_b;

	for (size_t i = 0; i < cases.len; i++)
		if (cases.items[i].is_def) {
			def_tgt = cases.items[i].blk;
			break;
		}

	/* collect non-default in order */
	int *nondef_idx = NULL;
	size_t nn = 0, nc = 0;

	for (size_t i = 0; i < cases.len; i++)
		if (!cases.items[i].is_def) {
			if (nn == nc) {
				size_t ncap = nc ? nc * 2 : 8;

				nondef_idx = (int *)xrealloc(
					nondef_idx, ncap * sizeof(int));
				nc = ncap;
			}

			nondef_idx[nn++] = (int)i;
		}

	L->cur = disp_b;

	if (cases.len == 0) {
		L->blocks.items[disp_b].term = TERM_GOTO;
		L->blocks.items[disp_b].target = end_b;
	} else if (nn == 0) {
		L->blocks.items[disp_b].term = TERM_GOTO;
		L->blocks.items[disp_b].target = def_tgt;
	} else {
		int cur_test = disp_b;

		for (size_t k = 0; k < nn; k++) {
			int ci = nondef_idx[k];
			int cblk = cases.items[ci].blk;
			char *cexpr = cases.items[ci].expr;
			int next_test = -1;

			if (k + 1 < nn) {
				next_test = lower_new_block(L);
			}

			int false_tgt = (next_test >= 0) ? next_test : def_tgt;

			/* cond: (tmp) == (cexpr) */
			StrBuf cb;

			sb_init(&cb);
			sb_puts(&cb, "(");
			sb_puts(&cb, tmp);
			sb_puts(&cb, ") == (");
			sb_puts(&cb, cexpr);
			sb_puts(&cb, ")");
			L->blocks.items[cur_test].term = TERM_COND;
			L->blocks.items[cur_test].cond = cb.data;
			L->blocks.items[cur_test].target = cblk;
			L->blocks.items[cur_test].target2 = false_tgt;

			if (next_test >= 0)
				cur_test = next_test;
		}
	}

	free(nondef_idx);
	for (size_t i = 0; i < cases.len; i++)
		if (cases.items[i].expr)
			free(cases.items[i].expr);
	free(cases.items);
	loop_pop(L);
	L->cur = end_b;
}

/**
 * @brief Lower one AST node into basic blocks.
 *
 * @param L Lowerer.
 * @param nd Node.
 */
void lower_stmt(Lowerer *L, Node *nd)
{
	int tp = nd->type;

	if (tp == N_BLOCK) {
		for (size_t i = 0; i < nd->stmts.len; i++)
			lower_stmt(L, nd->stmts.items[i]);
	} else if (tp == N_EMPTY) {
	} else if (tp == N_EXPR) {
		lower_emit(L, nd->expr_text);
	} else if (tp == N_IF) {
		int then_b = lower_new_block(L), else_b = lower_new_block(L),
		    end_b = lower_new_block(L);
		Block *cb = &L->blocks.items[L->cur];

		cb->term = TERM_COND;
		cb->cond = xstrdup(nd->cond);
		cb->target = then_b;
		cb->target2 = (nd->else_b ? else_b : end_b);
		L->cur = then_b;

		lower_stmt(L, nd->then_b);
		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = end_b;
			L->cur = lower_new_block(L);
		}

		if (nd->else_b) {
			L->cur = else_b;

			lower_stmt(L, nd->else_b);
			if (!lower_cur_term(L)) {
				L->blocks.items[L->cur].term = TERM_GOTO;
				L->blocks.items[L->cur].target = end_b;
				L->cur = lower_new_block(L);
			}
		} else {
			L->blocks.items[else_b].term = TERM_GOTO;
			L->blocks.items[else_b].target = end_b;
		}

		L->cur = end_b;
	} else if (tp == N_WHILE) {
		int cond_b = lower_new_block(L), body_b = lower_new_block(L),
		    end_b = lower_new_block(L);
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = cond_b;
		L->cur = cond_b;
		L->blocks.items[L->cur].term = TERM_COND;
		L->blocks.items[L->cur].cond = xstrdup(nd->wcond);
		L->blocks.items[L->cur].target = body_b;
		L->blocks.items[L->cur].target2 = end_b;
		L->cur = body_b;

		loop_push(L, 1, end_b, 1, cond_b);
		lower_stmt(L, nd->body);
		loop_pop(L);
		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = cond_b;
			L->cur = lower_new_block(L);
		}

		L->cur = end_b;
	} else if (tp == N_FOR) {
		if (nd->init_expr && nd->init_expr[0]) {
			const char *p = nd->init_expr;

			while (*p && isspace((unsigned char)*p))
				p++;
			if (*p) {
				StrBuf b;

				sb_init(&b);
				sb_puts(&b, nd->init_expr);
				/* ensure ; */
				char *s = b.data;
				size_t n = strlen(s);

				while (n && isspace((unsigned char)s[n - 1]))
					s[--n] = '\0';
				if (n == 0 || s[n - 1] != ';') {
					sb_puts(&b, " ;");
				}

				lower_emit(L, b.data);
				free(b.data);
			}
		}

		int cond_b = lower_new_block(L), body_b = lower_new_block(L),
		    incr_b = lower_new_block(L), end_b = lower_new_block(L);
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = cond_b;
		L->cur = cond_b;

		const char *cnd =
			(nd->for_cond && nd->for_cond[0]) ? nd->for_cond : "1";
		/* trim check empty */
		{
			const char *q = cnd;

			while (*q && isspace((unsigned char)*q))
				q++;
			if (!*q)
				cnd = "1";
		}

		L->blocks.items[L->cur].term = TERM_COND;
		L->blocks.items[L->cur].cond = xstrdup(cnd);
		L->blocks.items[L->cur].target = body_b;
		L->blocks.items[L->cur].target2 = end_b;
		L->cur = body_b;

		loop_push(L, 1, end_b, 1, incr_b);
		lower_stmt(L, nd->for_body);
		loop_pop(L);
		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = incr_b;
			L->cur = lower_new_block(L);
		}

		/* incr block */
		if (nd->incr && nd->incr[0]) {
			const char *p = nd->incr;

			while (*p && isspace((unsigned char)*p))
				p++;
			if (*p) {
				StrBuf b;

				sb_init(&b);
				sb_puts(&b, nd->incr);
				char *s = b.data;
				size_t n = strlen(s);

				while (n && isspace((unsigned char)s[n - 1]))
					s[--n] = '\0';
				if (s[n - 1] != ';')
					sb_puts(&b, " ;");
				sv_push(&L->blocks.items[incr_b].stmts, b.data);
			}
		}

		if (L->blocks.items[incr_b].term == TERM_NONE) {
			L->blocks.items[incr_b].term = TERM_GOTO;
			L->blocks.items[incr_b].target = cond_b;
		}

		L->cur = end_b;
	} else if (tp == N_DOWHILE) {
		int body_b = lower_new_block(L), cond_b = lower_new_block(L),
		    end_b = lower_new_block(L);
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = body_b;
		L->cur = body_b;

		loop_push(L, 1, end_b, 1, cond_b);
		lower_stmt(L, nd->body);
		loop_pop(L);
		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = cond_b;
			L->cur = lower_new_block(L);
		}

		L->cur = cond_b;
		L->blocks.items[L->cur].term = TERM_COND;
		L->blocks.items[L->cur].cond = xstrdup(nd->wcond);
		L->blocks.items[L->cur].target = body_b;
		L->blocks.items[L->cur].target2 = end_b;
		L->cur = end_b;
	} else if (tp == N_SWITCH) {
		lower_switch(L, nd);
	} else if (tp == N_CASE) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;
	} else if (tp == N_DEFAULT) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;
	} else if (tp == N_BREAK) {
		int tgt = -1;

		for (size_t i = L->llen; i > 0; i--)
			if (L->loops[i - 1].has_break) {
				tgt = L->loops[i - 1].brk;
				break;
			}

		if (tgt < 0) {
			L->blocks.items[L->cur].term = TERM_EXIT;
			L->cur = lower_new_block(L);
		} else {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = tgt;
			L->cur = lower_new_block(L);
		}
	} else if (tp == N_CONTINUE) {
		int tgt = -1;

		for (size_t i = L->llen; i > 0; i--)
			if (L->loops[i - 1].has_cont) {
				tgt = L->loops[i - 1].cont;
				break;
			}

		if (tgt < 0) {
			L->blocks.items[L->cur].term = TERM_EXIT;
			L->cur = lower_new_block(L);
		} else {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = tgt;
			L->cur = lower_new_block(L);
		}
	} else if (tp == N_GOTO) {
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = -2;
		L->blocks.items[L->cur].goto_label = xstrdup(nd->label);
		L->cur = lower_new_block(L);
	} else if (tp == N_LABEL) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		set_label(L, nd->label, nb);
	} else if (tp == N_LABELED) {
		int nb = lower_new_block(L);

		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = nb;
		}

		L->cur = nb;

		set_label(L, nd->label, nb);
		lower_stmt(L, nd->labeled_stmt);
	} else if (tp == N_RETURN) {
		L->blocks.items[L->cur].term = TERM_RETURN;

		L->blocks.items[L->cur].ret_expr =
			nd->ret_expr ? xstrdup(nd->ret_expr) : NULL;
		L->cur = lower_new_block(L);
	} else if (tp == N_DECL || tp == N_DECL_NOVAR || tp == N_TYPEDEF) {
		/* should be hoisted; ignore */
	}
}

/* lower entry: returns entry id */
/**
 * @brief Lower a body and finalize the block graph.
 *
 * @param L Lowerer.
 * @param body Root block node.
 *
 * @return Entry block id.
 */
int lower_run(Lowerer *L, Node *body)
{
	L->entry = lower_new_block(L);
	L->cur = L->entry;

	lower_stmt(L, body);
	if (L->blocks.items[L->cur].term == TERM_NONE) {
		L->blocks.items[L->cur].term = TERM_EXIT;
	}

	for (size_t i = 0; i < L->blocks.len; i++)
		if (L->blocks.items[i].term == TERM_NONE)
			L->blocks.items[i].term = TERM_EXIT;

	/* resolve label gotos */
	for (size_t i = 0; i < L->blocks.len; i++) {
		Block *b = &L->blocks.items[i];

		if (b->term == TERM_GOTO && b->target == -2) {
			int tgt = find_label(L, b->goto_label ? b->goto_label
							      : "");
			if (tgt < 0) {
				tgt = lower_new_block(L);
				L->blocks.items[tgt].term = TERM_EXIT;

				set_label(L, b->goto_label ? b->goto_label : "",
					  tgt);
			}

			b->target = tgt;
			free(b->goto_label);
			b->goto_label = NULL;
		}
	}

	/* BFS reachable */
	char *vis = (char *)xmalloc(L->blocks.len ? L->blocks.len : 1);

	memset(vis, 0, L->blocks.len);
	int *stack = (int *)xmalloc((L->blocks.len + 1) * sizeof(int));
	size_t sp = 0;

	stack[sp++] = L->entry;
	while (sp) {
		int x = stack[--sp];

		if (x == -1)
			continue;
		if (x < 0 || (size_t)x >= L->blocks.len)
			continue;
		if (vis[x])
			continue;
		vis[x] = 1;
		Block *b = &L->blocks.items[x];

		if (b->term == TERM_GOTO)
			stack[sp++] = b->target;
		else if (b->term == TERM_COND) {
			stack[sp++] = b->target;
			stack[sp++] = b->target2;
		}
	}

	/* keep only reachable */
	int *remap = (int *)xmalloc((L->blocks.len ? L->blocks.len : 1) *
				    sizeof(int));
	for (size_t i = 0; i < L->blocks.len; i++)
		remap[i] = -1;
	/* order: entry first, then sorted */
	int *order = (int *)xmalloc((L->blocks.len + 1) * sizeof(int));
	size_t on = 0;

	if (L->entry >= 0 && (size_t)L->entry < L->blocks.len && vis[L->entry])
		order[on++] = L->entry;
	for (size_t i = 0; i < L->blocks.len; i++)
		if ((int)i != L->entry && vis[i])
			order[on++] = i;
	for (size_t i = 0; i < on; i++)
		remap[order[i]] = (int)i;
	Block *nb = (Block *)xmalloc((on ? on : 1) * sizeof(Block));

	for (size_t i = 0; i < on; i++) {
		Block *s = &L->blocks.items[order[i]];
		Block *d = &nb[i];

		sv_init(&d->stmts);
		for (size_t k = 0; k < s->stmts.len; k++)
			sv_push(&d->stmts, xstrdup(s->stmts.items[k]));
		d->term = s->term;
		d->cond = s->cond ? s->cond : NULL; /* transfer */
		s->cond = NULL;
		d->ret_expr = s->ret_expr ? s->ret_expr : NULL;
		s->ret_expr = NULL;
		d->goto_label = NULL;
		if (d->term == TERM_GOTO)
			d->target = remap[s->target];
		else if (d->term == TERM_COND) {
			d->target = remap[s->target];
			d->target2 = remap[s->target2];
		} else {
			d->target = s->target;
			d->target2 = s->target2;
		}
	}

	/* free old */
	for (size_t i = 0; i < L->blocks.len; i++) {
		for (size_t k = 0; k < L->blocks.items[i].stmts.len; k++)
			free(L->blocks.items[i].stmts.items[k]);
		free(L->blocks.items[i].stmts.items);
		if (L->blocks.items[i].cond)
			free(L->blocks.items[i].cond);
		if (L->blocks.items[i].ret_expr)
			free(L->blocks.items[i].ret_expr);
		if (L->blocks.items[i].goto_label)
			free(L->blocks.items[i].goto_label);
	}

	free(L->blocks.items);
	L->blocks.items = nb;
	L->blocks.len = on;
	L->blocks.cap = on;

	for (size_t i = 0; i < L->labels.len; i++) {
		int old = L->labels.items[i].bid;

		if (old >= 0 && (size_t)old < on * 2) { /* remap if reachable */
			/* find: old id -> new via remap (remap sized old len,
			 * but old len lost; approximate: search order) */
			int nn = -1;

			for (size_t k = 0; k < on; k++)
				if (order[k] == old) {
					nn = (int)k;
					break;
				}

			L->labels.items[i].bid = nn;
		}
	}

	L->entry = (on > 0) ? 0 : -1;

	free(vis);
	free(stack);
	free(remap);
	free(order);
	return L->entry;
}

/* ---------------- emission ---------------- */
/**
 * @brief Pick a dispatcher name free of collisions.
 *
 * @param h Hoister.
 *
 * @return New string.
 */
char *sanitize_state(Hoister *h)
{
	if (!sv_contains(&h->hoisted_names, "__cf_state"))
		return xstrdup("__cf_state");
	int i = 1;
	char buf[128];

	while (1) {
		snprintf(buf, sizeof(buf), "__cf_state_%d", i);
		if (!sv_contains(&h->hoisted_names, buf))
			return xstrdup(buf);
		i++;
	}
}

/**
 * @brief Render a flattened function with delta transitions.
 *
 * @param hdr Header tokens.
 * @param hn Header token count.
 * @param h Hoister.
 * @param L Lowerer.
 * @param state Dispatcher variable.
 *
 * @return New function text.
 */
char *emit_function(Token *hdr, size_t hn, Hoister *h, Lowerer *L,
		    const char *state)
{
	StrBuf o;

	sb_init(&o);
	char *hs = toks_to_str(hdr, hn);

	sb_puts(&o, hs);
	free(hs);
	sb_puts(&o, "\n{\n");
	for (size_t i = 0; i < h->hoisted.len; i++) {
		sb_puts(&o, "  ");
		sb_puts(&o, h->hoisted.items[i]);
		sb_putc(&o, '\n');
	}

	if (h->hoisted.len)
		sb_putc(&o, '\n');
	sb_puts(&o, "  int ");
	sb_puts(&o, state);
	sb_puts(&o, " = ");
	{
		char b[32];

		snprintf(b, sizeof(b), "%d", L->entry);
		sb_puts(&o, b);
	}

	sb_puts(&o, " ;\n");
	sb_puts(&o, "  while (");
	sb_puts(&o, state);
	sb_puts(&o, " != -1)\n  {\n");
	sb_puts(&o, "    switch (");
	sb_puts(&o, state);
	sb_puts(&o, ")\n    {\n");
	for (size_t bid = 0; bid < L->blocks.len; bid++) {
		Block *b = &L->blocks.items[bid];
		char tmp[64];

		snprintf(tmp, sizeof(tmp), "      case %zu:\n      {\n",
			 (size_t)bid);

		sb_puts(&o, tmp);
		for (size_t k = 0; k < b->stmts.len; k++) {
			const char *s = b->stmts.items[k];

			while (*s && isspace((unsigned char)*s))
				s++;
			if (!*s)
				continue;
			sb_puts(&o, "        ");
			sb_puts(&o, s);
			sb_putc(&o, '\n');
		}

		if (b->term == TERM_GOTO) {
			int delta = b->target - (int)bid;
			char t2[64];

			snprintf(t2, sizeof(t2), "        %s += %d ;\n", state,
				 delta);
			sb_puts(&o, t2);
			sb_puts(&o, "        break ;\n");
		} else if (b->term == TERM_COND) {
			int dt = b->target - (int)bid,
			    df = b->target2 - (int)bid;
			sb_puts(&o, "        ");
			sb_puts(&o, state);
			sb_puts(&o, " += ((");
			sb_puts(&o, b->cond);
			sb_puts(&o, ")) ? (");
			char t2[64];

			snprintf(t2, sizeof(t2), "%d", dt);
			sb_puts(&o, t2);
			sb_puts(&o, ") : (");
			snprintf(t2, sizeof(t2), "%d", df);
			sb_puts(&o, t2);
			sb_puts(&o, ") ;\n        break ;\n");
		} else if (b->term == TERM_RETURN) {
			if (!b->ret_expr || !b->ret_expr[0])
				sb_puts(&o, "        return ;\n");
			else {
				sb_puts(&o, "        return (");
				sb_puts(&o, b->ret_expr);
				sb_puts(&o, ") ;\n");
			}
		} else if (b->term == TERM_EXIT) {
			int delta = -1 - (int)bid;
			char t2[96];

			snprintf(t2, sizeof(t2),
				 "        %s += %d ;\n        break ;\n", state,
				 delta);
			sb_puts(&o, t2);
		}

		sb_puts(&o, "      }\n");
	}

	sb_puts(&o, "      default:\n      {\n        ");
	sb_puts(&o, state);
	sb_puts(&o, " += -1 - ");
	sb_puts(&o, state);
	sb_puts(&o, " ;\n        break ;\n      }\n");
	sb_puts(&o, "    }\n  }\n}\n");
	return o.data;
}

/* ---------------- flatten one function ---------------- */
/**
 * @brief Parse, hoist, lower and emit one function.
 *
 * @param header Header tokens.
 * @param body Braced body.
 * @param global_td Typedef set, extended in place.
 *
 * @return New function text.
 */
char *flatten_function(TokVec *header, TokVec *body, StrVec *global_td)
{
	/* body includes braces */
	if (body->len < 2)
		return NULL;
	TokVec inner;

	tv_init(&inner);
	for (size_t i = 1; i + 1 < body->len; i++)
		tv_push(&inner, body->items[i].kind,
			xstrdup(body->items[i].text));
	Parser p;

	p.toks = inner.items;
	p.n = inner.len;
	p.pos = 0;
	StrVec local_td;

	sv_init(&local_td);
	for (size_t i = 0; i < global_td->len; i++)
		sv_push(&local_td, xstrdup(global_td->items[i]));
	p.typedefs = &local_td;
	Node *root = node_new(N_BLOCK);

	nv_init(&root->stmts);
	while (!p_eof(&p)) {
		Node *s = parse_statement(&p);

		if (s) {
			if (s->type == N_BLOCK && 0) {} /* blocks stay */
			nv_push(&root->stmts, s);
		}
	}

	Hoister h;

	hoister_init(&h, &local_td);
	/* hoist: root stmts -> new list */
	NodeVec newlist;

	nv_init(&newlist);
	for (size_t i = 0; i < root->stmts.len; i++) {
		NodeVec r = hoist_node(&h, root->stmts.items[i]);

		nv_extend(&newlist, &r);
		free(r.items);
	}

	Node *newroot = node_new(N_BLOCK);

	newroot->stmts = newlist;
	Lowerer L;

	lower_init(&L, &h);
	lower_run(&L, newroot);
	char *state = sanitize_state(&h);
	char *code = emit_function(header->items, header->len, &h, &L, state);

	free(state);
	/* update global typedefs with new ones */
	for (size_t i = 0; i < local_td.len; i++)
		if (!sv_contains(global_td, local_td.items[i]))
			sv_push(global_td, xstrdup(local_td.items[i]));
	/* leak most for brevity */
	tv_free(&inner);
	return code;
}

/**
 * @brief Record typedef names from a declaration.
 *
 * @param txt Declaration text.
 * @param out Name set.
 */
void collect_typedefs_from_text(const char *txt, StrVec *out)
{
	TokVec tv = tokenize(txt);

	for (size_t i = 0; i < tv.len; i++)
		if (streq(tv.items[i].text, "typedef")) {
			size_t j = i + 1;
			const char *last = NULL;

			while (j < tv.len && !streq(tv.items[j].text, ";")) {
				if (tv.items[j].kind == TOK_IDENT)
					last = tv.items[j].text;
				j++;
			}

			if (last && !sv_contains(out, last))
				sv_push(out, xstrdup(last));
		}

	tv_free(&tv);
}

/**
 * @brief Flatten every function of a program.
 *
 * @param src Source text.
 *
 * @return New program; directives pass through.
 */
char *flatten_program(const char *src)
{
	SegVec segs = split_preproc(src);
	StrBuf out;

	sb_init(&out);
	sb_puts(&out,
		"/* Flattened by cflatten (C port) - control flow flattened "
		"into dispatcher loop */\n");
	StrVec gtd;

	sv_init(&gtd);
	for (size_t si = 0; si < segs.len; si++) {
		if (segs.items[si].is_preproc) {
			sb_puts(&out, segs.items[si].text);
		} else {
			const char *code = segs.items[si].text;
			int blank = 1;

			for (const char *q = code; *q; q++)
				if (!isspace((unsigned char)*q)) {
					blank = 0;
					break;
				}

			if (blank)
				continue;
			TLVect tl = extract_toplevel(code);

			for (size_t i = 0; i < tl.len; i++)
				if (tl.items[i].kind == TL_OTHER)
					collect_typedefs_from_text(
						tl.items[i].text, &gtd);
			for (size_t i = 0; i < tl.len; i++) {
				TLItem *it = &tl.items[i];

				if (it->kind == TL_FUNC) {
					char *flat = NULL;

					/* try flatten; on failure pass through
					 */
					/* no exceptions in C; assume success */
					flat = flatten_function(
						&it->header, &it->body, &gtd);
					if (flat) {
						sb_puts(&out, flat);
						sb_putc(&out, '\n');
						free(flat);
					} else {
						char *hs = toks_to_str(
							it->header.items,
							it->header.len);
						char *bs = toks_to_str(
							it->body.items,
							it->body.len);
						sb_puts(&out,
							"/* cflatten: "
							"passthrough */\n");
						sb_puts(&out, hs);
						sb_putc(&out, ' ');
						sb_puts(&out, bs);
						sb_putc(&out, '\n');
						free(hs);
						free(bs);
					}
				} else {
					sb_puts(&out, it->text);
					sb_putc(&out, '\n');
				}
			}

			/* leak tl for brevity */
		}
	}

	return out.data;
}
