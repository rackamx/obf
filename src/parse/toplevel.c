/**
 * @file toplevel.c
 * @brief Toplevel splitter: function definitions versus others.
 */

#include "parse/toplevel.h"
#include "parse/token.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

	/* First: the header must hold a balanced (...) pair, i.e. a
	 * parameter list.  Anything else cannot be a function. */
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
	/* Next: no depth-0 ';' or '=' may appear.  Those mark variable
	 * definitions or prototypes handled elsewhere. */
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
	/* Finally the identifier before the parameter list is the name,
	 * skipping any '*' of pointer declarators.  Control keywords such
	 * as "if" can never start a function header. */
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

	/* cur marks the start of the declaration being scanned. */
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

				/* Consume the body up to its matching '}'. */
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
					/* Unbalanced input: keep the rest as
					 * plain text rather than losing it. */
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
				/* Header is everything before '{', body keeps
				 * its braces.  Token texts are duplicated so
				 * the item owns them. */
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

				/* Not a function (e.g. "struct { ... };"): skip
				 * the balanced block as part of plain text. */
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

				/* A declaration such as "struct {...};" may
				 * trail the block, so stretch to the closing
				 * ';'. */
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
			/* End of a plain declaration: emit it as other text. */
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
			/* Stray closer outside any function: keep it verbatim.
			 */
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
		/* Trailing tokens without a terminator: keep them only if
		 * they hold something beyond whitespace. */
		int any = 0;

		for (size_t k = cur; k < n; k++) {
			if (strlen(toks.items[k].text) > 0) {
				any = 1;
				break;
			}
		}

		if (any) {
			char *txt = toks_to_str(toks.items + cur, n - cur);

			/* The token join may still be blank; drop it then. */
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
