/**
 * @file flatten.c
 * @brief Control-flow flattening driver.
 */

#include "cff/flatten.h"
#include "cff/emit.h"
#include "cff/hoister.h"
#include "cff/lowerer.h"
#include "parse/ast.h"
#include "parse/parser.h"
#include "parse/segment.h"
#include "parse/token.h"
#include "parse/toplevel.h"
#include "utils/strbuf.h"
#include "utils/strvec.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

	/* Strip the outer braces; the parser wants statements. */
	tv_init(&inner);
	for (size_t i = 1; i + 1 < body->len; i++)
		tv_push(&inner, body->items[i].kind,
			xstrdup(body->items[i].text));
	Parser p;

	p.toks = inner.items;
	p.n = inner.len;
	p.pos = 0;
	StrVec local_td;

	/* Each function starts from the globals plus whatever its own
	 * typedefs add along the way. */
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

	/* New typedefs met inside flow back to later functions. */
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

	/* After each "typedef", the new name is the last identifier
	 * before ';', e.g. the "T" in "typedef struct {...} T;". */
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

			/* Typedefs are collected first so declarations in
			 * later items already know the type names. */
			for (size_t i = 0; i < tl.len; i++)
				if (tl.items[i].kind == TL_OTHER)
					collect_typedefs_from_text(
						tl.items[i].text, &gtd);
			for (size_t i = 0; i < tl.len; i++) {
				TLItem *it = &tl.items[i];

				if (it->kind == TL_FUNC) {
					char *flat = NULL;

					/* A function that resists flattening
					 * passes through as written. */
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
