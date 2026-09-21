/**
 * @file emit.c
 * @brief Dispatcher emission with delta state transitions.
 */

#include "cff/emit.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Pick a dispatcher name free of collisions.
 *
 * @param h Hoister.
 *
 * @return New string.
 */
char *sanitize_state(Hoister *h)
{
	/* The common case needs no suffix; only an actual collision
	 * forces "__cf_state_1" and friends. */
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

	/* The original header is reused verbatim: parameters stay visible
	 * to every case below. */
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

		/* Every transition is a delta from the current case id:
		 * the switch guarantees state == bid on entry, so adding
		 * (target - bid) lands exactly on target. */
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

	/* Default catches impossible states; its delta is computed at
	 * runtime since the entry value is unknown. */
	sb_puts(&o, "      default:\n      {\n        ");
	sb_puts(&o, state);
	sb_puts(&o, " += -1 - ");
	sb_puts(&o, state);
	sb_puts(&o, " ;\n        break ;\n      }\n");
	sb_puts(&o, "    }\n  }\n}\n");
	return o.data;
}
