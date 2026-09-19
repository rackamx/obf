/**
 * @file parser.c
 * @brief C statement-level parser: tokenizer and AST builder.
 */
#include "parser.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ---------------- keywords ---------------- */
static const char *KEYWORDS[] = {
	"auto",	      "break",	   "case",	     "char",
	"const",      "continue",  "default",	     "do",
	"double",     "else",	   "enum",	     "extern",
	"float",      "for",	   "goto",	     "if",
	"inline",     "int",	   "long",	     "register",
	"restrict",   "return",	   "short",	     "signed",
	"sizeof",     "static",	   "struct",	     "switch",
	"typedef",    "union",	   "unsigned",	     "void",
	"volatile",   "while",	   "_Bool",	     "_Complex",
	"_Imaginary", "_Alignas",  "_Alignof",	     "_Atomic",
	"_Generic",   "_Noreturn", "_Static_assert", "_Thread_local",
	NULL};

static const char *TYPE_KWS[] = {
	"void",	    "char",	 "short",      "int",	   "long",
	"float",    "double",	 "signed",     "unsigned", "struct",
	"union",    "enum",	 "const",      "volatile", "static",
	"extern",   "register",	 "auto",       "inline",   "restrict",
	"_Bool",    "_Complex",	 "_Imaginary", "_Atomic",  "_Thread_local",
	"_Alignas", "typeof",	 "__typeof__", "__typeof", "size_t",
	"ssize_t",  "_Noreturn", NULL};
/**
 * @brief Test whether a word is a C keyword.
 *
 * @param w Word to test.
 *
 * @return 1 for keywords, 0 otherwise.
 */
int is_keyword(const char *w)
{
	for (int i = 0; KEYWORDS[i]; i++)
		if (strcmp(KEYWORDS[i], w) == 0)
			return 1;
	return 0;
}
/**
 * @brief Test whether a word may open a declaration.
 *
 * @param w Word to test.
 *
 * @return 1 for type/qualifier keywords.
 */
int is_type_kw(const char *w)
{
	for (int i = 0; TYPE_KWS[i]; i++)
		if (strcmp(TYPE_KWS[i], w) == 0)
			return 1;
	return 0;
}
/* ---------------- tokens ---------------- */
/**
 * @brief Initialise an empty token vector.
 *
 * @param v Vector to initialise.
 */
void tv_init(TokVec *v)
{
	v->items = NULL;
	v->len = 0;
	v->cap = 0;
}
/**
 * @brief Append a token, taking ownership of the text.
 *
 * @param v Vector.
 * @param kind Token kind.
 * @param text Token text now owned by the vector.
 */
void tv_push(TokVec *v, int kind, char *text)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 64;

		v->items = (Token *)xrealloc(v->items, nc * sizeof(Token));
		v->cap = nc;
	}

	v->items[v->len].kind = kind;
	v->items[v->len].text = text;
	v->len++;
}
/**
 * @brief Release a token vector and all its texts.
 *
 * @param v Vector to release.
 */
void tv_free(TokVec *v)
{
	for (size_t i = 0; i < v->len; i++)
		free(v->items[i].text);
	free(v->items);
	v->items = NULL;
	v->len = v->cap = 0;
}
/* strip comments -> spaces (same length, preserves strings) */
/**
 * @brief Blank out comments, preserving strings and length.
 *
 * @param src Source text.
 *
 * @return New string of the same length.
 */
char *strip_comments(const char *src)
{
	size_t n = strlen(src);
	char *out = (char *)xmalloc(n + 1);
	size_t i = 0;
	int in_s = 0, in_c = 0, esc = 0;

	while (i < n) {
		char ch = src[i];

		if (in_s) {
			out[i] = ch;
			if (esc)
				esc = 0;
			else if (ch == '\\')
				esc = 1;
			else if (ch == '"')
				in_s = 0;
			i++;
		} else if (in_c) {
			out[i] = ch;
			if (esc)
				esc = 0;
			else if (ch == '\\')
				esc = 1;
			else if (ch == '\'')
				in_c = 0;
			i++;
		} else {
			if (ch == '"') {
				in_s = 1;
				out[i] = ch;
				i++;
			} else if (ch == '\'') {
				in_c = 1;
				out[i] = ch;
				i++;
			} else if (ch == '/' && i + 1 < n &&

				   src[i + 1] == '/') {
				while (i < n && src[i] != '\n') {
					out[i] = (src[i] == '\n' ||
						  src[i] == '\r')
							 ? src[i]
							 : ' ';
					i++;
				}
			} else if (ch == '/' && i + 1 < n &&

				   src[i + 1] == '*') {
				out[i] = ' ';
				out[i + 1] = ' ';
				i += 2;
				while (i < n && !(src[i] == '*' && i + 1 < n &&
						  src[i + 1] == '/')) {
					out[i] = (src[i] == '\n' ||
						  src[i] == '\r')
							 ? src[i]
							 : ' ';
					i++;
				}

				if (i < n) {
					out[i] = ' ';
					if (i + 1 < n)
						out[i + 1] = ' ';
					i += 2;
				}
			} else {
				out[i] = ch;
				i++;
			}
		}
	}

	out[n] = '\0';
	return out;
}
/**
 * @brief Test for identifier-first characters.
 *
 * @param c Character to test.
 *
 * @return 1 when valid, 0 otherwise.
 */
int is_ident_start(char c)
{
	return isalpha((unsigned char)c) || c == '_' || c == '$';
}
/**
 * @brief Test for identifier characters.
 *
 * @param c Character to test.
 *
 * @return 1 when valid, 0 otherwise.
 */
int is_ident_char(char c)
{
	return isalnum((unsigned char)c) || c == '_' || c == '$';
}
/**
 * @brief Split C source into a token stream.
 *
 * @param code Source text to scan.
 *
 * @return Token vector; every text is newly allocated.
 */
TokVec tokenize(const char *code)
{
	TokVec v;

	tv_init(&v);
	size_t n = strlen(code), i = 0;

	static const char *MULTI[] = {
		"...", "<<=", ">>=", "->", "++", "--", "<<", ">>", "<=",
		">=",  "==",  "!=",  "&&", "||", "+=", "-=", "*=", "/=",
		"%=",  "&=",  "|=",  "^=", "##", "::", NULL};
	while (i < n) {
		char c = code[i];

		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
		    c == '\v' || c == '\f') {
			i++;
			continue;
		}

		if (c == '"') {
			size_t j = i + 1;
			int esc = 0;

			while (j < n) {
				if (esc)
					esc = 0;
				else if (code[j] == '\\')
					esc = 1;
				else if (code[j] == '"')
					break;
				j++;
			}

			if (j < n)
				j++;
			tv_push(&v, TOK_STRING, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		if (c == '\'') {
			size_t j = i + 1;
			int esc = 0;

			while (j < n) {
				if (esc)
					esc = 0;
				else if (code[j] == '\\')
					esc = 1;
				else if (code[j] == '\'')
					break;
				else if (code[j] == '\n')
					break;
				j++;
			}

			if (j < n && code[j] == '\'')
				j++;
			tv_push(&v, TOK_CHAR, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		if (is_ident_start(c)) {
			size_t j = i + 1;

			while (j < n && is_ident_char(code[j]))
				j++;
			char *w = xstrndup(code + i, j - i);

			tv_push(&v, is_keyword(w) ? TOK_KEYWORD : TOK_IDENT, w);
			i = j;
			continue;
		}

		if (isdigit((unsigned char)c) ||
		    (c == '.' && i + 1 < n &&
		     isdigit((unsigned char)code[i + 1]))) {
			size_t j = i;

			while (j < n && (isalnum((unsigned char)code[j]) ||
					 code[j] == '_' || code[j] == '.' ||
					 code[j] == '\''))
				j++;
			tv_push(&v, TOK_NUMBER, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		int matched = 0;

		for (int k = 0; MULTI[k]; k++) {
			size_t L = strlen(MULTI[k]);

			if (i + L <= n && memcmp(code + i, MULTI[k], L) == 0) {
				tv_push(&v, TOK_PUNCT, xstrdup(MULTI[k]));
				i += L;
				matched = 1;
				break;
			}
		}

		if (matched)
			continue;
		tv_push(&v, TOK_PUNCT, xstrndup(code + i, 1));
		i++;
	}

	return v;
}
/**
 * @brief Join tokens with single spaces.
 *
 * @param toks Tokens.
 * @param n Token count.
 *
 * @return New string.
 */
char *toks_to_str(Token *toks, size_t n)
{
	StrBuf b;

	sb_init(&b);
	for (size_t i = 0; i < n; i++) {
		if (i)
			sb_putc(&b, ' ');
		sb_puts(&b, toks[i].text);
	}

	return b.data;
}
/**
 * @brief Join a token vector with single spaces.
 *
 * @param v Tokens.
 *
 * @return New string.
 */
char *tokvec_to_str(TokVec *v)
{
	return toks_to_str(v->items, v->len);
}
/* ---------------- preproc split ---------------- */
/**
 * @brief Append a source segment.
 *
 * @param v Vector.
 * @param is_pre Non-zero for preprocessor lines.
 * @param t Segment text, ownership taken.
 */
void segvec_push(SegVec *v, int is_pre, char *t)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 8;

		v->items = (Seg *)xrealloc(v->items, nc * sizeof(Seg));
		v->cap = nc;
	}

	v->items[v->len].is_preproc = is_pre;
	v->items[v->len].text = t;
	v->len++;
}
/**
 * @brief Split source into code/preprocessor segments.
 *
 * @param src Source text.
 *
 * @return Segments in source order.
 */
SegVec split_preproc(const char *src)
{
	SegVec v;

	v.items = NULL;
	v.len = 0;
	v.cap = 0;
	/* split by lines */
	StrBuf cur;

	sb_init(&cur);
	const char *p = src;

	while (1) {
		const char *nl = strchr(p, '\n');
		size_t ll = nl ? (size_t)(nl - p) : strlen(p);
		char *line = xstrndup(p, ll);
		/* lstrip check */
		const char *q = line;

		while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\v' ||
		       *q == '\f')
			q++;
		if (*q == '#') {
			if (cur.len > 0) {
				segvec_push(&v, 0, cur.data);
				sb_init(&cur);
			}

			StrBuf pl;

			sb_init(&pl);
			sb_puts(&pl, line);
			sb_putc(&pl, '\n');
			segvec_push(&v, 1, pl.data);
		} else {
			sb_putn(&cur, line, ll);
			sb_putc(&cur, '\n');
		}

		free(line);
		if (!nl)
			break;
		p = nl + 1;
	}

	if (cur.len > 0)
		segvec_push(&v, 0, cur.data);
	else
		free(cur.data);
	return v;
}
/* ---------------- toplevel ---------------- */
/**
 * @brief Append a toplevel item.
 *
 * @param v Vector.
 * @param it Item to append.
 */
void tlv_push(TLVect *v, TLItem it)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 8;

		v->items = (TLItem *)xrealloc(v->items, nc * sizeof(TLItem));
		v->cap = nc;
	}

	v->items[v->len++] = it;
}
/**
 * @brief Heuristic function-definition header test.
 *
 * @param h Tokens before '{'.
 * @param n Token count.
 *
 * @return Function name, or NULL when not a definition.
 */
const char *is_func_header(Token *h, size_t n)
{
	if (n == 0)
		return NULL;
	int depth = 0, saw = 0;

	for (size_t i = 0; i < n; i++) {
		if (streq(h[i].text, "("))
			depth++;
		else if (streq(h[i].text, ")")) {
			depth--;
			if (depth == 0)
				saw = 1;
		}
	}

	if (!saw)
		return NULL;
	depth = 0;
	for (size_t i = 0; i < n; i++) {
		if (streq(h[i].text, "(") || streq(h[i].text, "[") ||
		    streq(h[i].text, "{"))
			depth++;
		else if (streq(h[i].text, ")") || streq(h[i].text, "]") ||
			 streq(h[i].text, "}"))
			depth--;
		if (depth == 0 &&
		    (streq(h[i].text, ";") || streq(h[i].text, "=")))
			return NULL;
	}

	depth = 0;
	for (size_t i = 0; i < n; i++) {
		if (streq(h[i].text, "(") && depth == 0) {
			int j = (int)i - 1;

			while (j >= 0 && streq(h[j].text, "*"))
				j--;
			if (j >= 0 && (h[j].kind == TOK_IDENT ||
				       h[j].kind == TOK_KEYWORD)) {
				const char *nm = h[j].text;

				if (streq(nm, "if") || streq(nm, "while") ||
				    streq(nm, "for") || streq(nm, "switch") ||
				    streq(nm, "return") ||
				    streq(nm, "sizeof") ||
				    streq(nm, "typeof") ||
				    streq(nm, "__typeof__"))
					return NULL;
				return nm;
			}

			return NULL;
		}

		if (streq(h[i].text, "(") || streq(h[i].text, "[") ||
		    streq(h[i].text, "{"))
			depth++;
		else if (streq(h[i].text, ")") || streq(h[i].text, "]") ||
			 streq(h[i].text, "}"))
			depth--;
	}

	return NULL;
}
/**
 * @brief Split code into function and other declarations.
 *
 * @param code Code without preprocessor lines.
 *
 * @return Toplevel items; bodies keep their braces.
 */
TLVect extract_toplevel(const char *code)
{
	TLVect out;

	out.items = NULL;
	out.len = 0;
	out.cap = 0;
	char *stripped = strip_comments(code);
	TokVec toks = tokenize(stripped);

	free(stripped);
	size_t n = toks.len, i = 0, cur = 0;

	while (i < n) {
		Token *t = &toks.items[i];

		if (streq(t->text, "{")) {
			size_t hn = i - cur;
			const char *fn = NULL;

			if (hn > 0)
				fn = is_func_header(toks.items + cur, hn);
			if (fn) {
				char *fname = xstrdup(fn);
				int depth = 0;
				size_t j = i;

				while (j < n) {
					if (streq(toks.items[j].text, "{"))
						depth++;
					else if (streq(toks.items[j].text,
						       "}")) {
						depth--;
						if (depth == 0)
							break;
					}

					j++;
				}

				if (j >= n) {
					char *txt = toks_to_str(
						toks.items + cur, n - cur);
					TLItem it;

					it.kind = TL_OTHER;
					it.text = txt;
					it.name = NULL;
					it.header.items = NULL;
					it.header.len = it.header.cap = 0;
					it.body.items = NULL;
					it.body.len = it.body.cap = 0;
					tlv_push(&out, it);
					break;
				}

				TLItem it;

				it.kind = TL_FUNC;
				it.text = NULL;
				it.name = fname;
				tv_init(&it.header);
				tv_init(&it.body);
				for (size_t k = cur; k < i; k++)
					tv_push(&it.header, toks.items[k].kind,
						xstrdup(toks.items[k].text));
				for (size_t k = i; k <= j; k++)
					tv_push(&it.body, toks.items[k].kind,
						xstrdup(toks.items[k].text));
				tlv_push(&out, it);
				i = j + 1;
				cur = i;
				continue;
			} else {
				int depth = 0;
				size_t j = i;

				while (j < n) {
					if (streq(toks.items[j].text, "{"))
						depth++;
					else if (streq(toks.items[j].text,
						       "}")) {
						depth--;
						if (depth == 0)
							break;
					}

					j++;
				}

				j = (j < n) ? j + 1 : n;
				/* extend to ';' handling */
				while (j < n &&
				       !streq(toks.items[j].text, ";") &&
				       !streq(toks.items[j].text, "{")) {
					if (streq(toks.items[j].text, ";")) {
						j++;
						break;
					}

					j++;
					if (j < n &&
					    streq(toks.items[j].text, ";")) {
						j++;
						break;
					}

					if (j < n &&
					    streq(toks.items[j].text, "{"))
						break;
				}

				i = j;
				continue;
			}
		} else if (streq(t->text, ";")) {
			char *txt = toks_to_str(toks.items + cur, i - cur + 1);
			TLItem it;

			it.kind = TL_OTHER;
			it.text = txt;
			it.name = NULL;
			it.header.items = NULL;
			it.header.len = it.header.cap = 0;
			it.body.items = NULL;
			it.body.len = it.body.cap = 0;
			tlv_push(&out, it);
			i++;
			cur = i;
			continue;
		} else if (streq(t->text, "}")) {
			char *txt = toks_to_str(toks.items + cur, i - cur + 1);
			TLItem it;

			it.kind = TL_OTHER;
			it.text = txt;
			it.name = NULL;
			it.header.items = NULL;
			it.header.len = it.header.cap = 0;
			it.body.items = NULL;
			it.body.len = it.body.cap = 0;
			tlv_push(&out, it);
			i++;
			cur = i;
			continue;
		} else

			i++;
	}

	if (cur < n) {
		/* trim whitespace-only? */
		int any = 0;

		for (size_t k = cur; k < n; k++) {
			if (strlen(toks.items[k].text) > 0) {
				any = 1;
				break;
			}
		}

		if (any) {
			char *txt = toks_to_str(toks.items + cur, n - cur);
			/* check non-blank */
			int blank = 1;

			for (char *p = txt; *p; p++)
				if (!isspace((unsigned char)*p)) {
					blank = 0;
					break;
				}

			if (!blank) {
				TLItem it;

				it.kind = TL_OTHER;
				it.text = txt;
				it.name = NULL;
				it.header.items = NULL;
				it.header.len = it.header.cap = 0;
				it.body.items = NULL;
				it.body.len = it.body.cap = 0;
				tlv_push(&out, it);
			} else

				free(txt);
		}
	}

	tv_free(&toks);
	return out;
}
/* ---------------- AST ---------------- */
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
/* parser */
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
/* forward decls */
Node *parse_statement(Parser *p);

Node *parse_declaration(Parser *p);

TokVec capture_until_semi(Parser *p);
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
/* decl name helpers */
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
	/* collect (depth, text) for idents with brack==0 */
	typedef struct {
		int d;
		const char *t;
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
	/* find idx from end with depth 0 */
	int *dps = (int *)xmalloc(n * sizeof(int));
	int *dbs = (int *)xmalloc(n * sizeof(int));
	int dp = 0, db = 0;

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

	for (int k = (int)n - 1; k >= 0; k--)
		if (toks[k].kind == TOK_IDENT && streq(toks[k].text, name) &&
		    dps[k] == 0) {
			idx = k;
			break;
		}

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
		/* trim leading space */
		char *tgs = gs;

		while (*tgs == ' ')
			tgs++;
		if (!first)
			sb_putc(&b, ' ');
		sb_puts(&b, tgs);
		first = 0;
		free(gs);
	}
	/* normalize: remove spaces? keep as is; fix_array_suffix handles spaces
	 */
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
	/* assumes '(' already consumed; capture until matching ')' */
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
	/* trim */
	char *init_s = sa, *cond_s = sb2, *incr_s = sc;
	char *init_decl = NULL, *init_expr = NULL;

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
		if (isd)
			free(cond_s == init_s ? NULL : NULL); /* keep */
	}

	Node *body = parse_statement(p);
	Node *n = node_new(N_FOR);

	n->init_decl = init_decl;
	n->init_expr = init_expr;
	/* if init was decl, cond_s/incr still owned; if init expr, same */
	if (!init_decl && !init_expr) {
		free(init_s);
	}

	n->for_cond = cond_s;
	n->incr = incr_s;
	n->for_body = body;
	/* if init empty and we freed? careful: init_s freed only when empty */
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
/* declaration parsing */
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
		tv_free(&toks);
		p->pos = start;
		return NULL;
	}
	/* body without trailing ; */
	size_t bn = toks.len - 1;

	if (bn == 0) { /* empty? */
	}
	/* split by top-level commas */
	/* build chunks as TokVec array */
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
	/* find type_end in first chunk */
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
			i2++;
			type_end = i2;
			continue;
		} else if (t->kind == TOK_IDENT && i2 == 0 && cn == 1) {
			fail = 1;
			break;
		} else

			break;
	}

	if (type_end == 0)
		fail = 1;
	if (fail) {
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
	/* check novar: no declarator and single chunk */
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
	/* build decl chunks */
	Node *nd = node_new(N_DECL);

	nd->type_str = type_str;
	nd->decls.items = NULL;
	nd->decls.len = nd->decls.cap = 0;
	/* first remainder */
	/* collect declarator token lists */
	typedef struct {
		Token *t;
		size_t n;
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
		/* trim */
		Node *n = node_new(N_RETURN);
		/* strip trailing spaces: toks_to_str already trimmed? keep as
		 * is, trim */
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

		if (c1 && streq(c1, ":") && !(c2 && streq(c2, ":"))) {
			char *lname = xstrdup(t->text);

			p_next(p);
			p_next(p);
			const char *nx = p_peekt(p, 0);

			if (!nx || streq(nx, "}")) {
				Node *n = node_new(N_LABEL);

				n->label = lname;
				return n;
			}

			if (p_eof(p)) {
				Node *n = node_new(N_LABEL);

				n->label = lname;
				return n;
			}

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
		p->pos = save;
	}

	TokVec v = capture_until_semi(p);

	if (v.len == 0) {
		tv_free(&v);
		return node_new(N_EMPTY);
	}

	char *s = tokvec_to_str(&v);

	tv_free(&v);
	/* trim check empty */
	{
		int blank = 1;

		for (char *q = s; *q; q++)
			if (!isspace((unsigned char)*q)) {
				blank = 0;
				break;
			}

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
	n->expr_text = b.data;
	return n;
}
