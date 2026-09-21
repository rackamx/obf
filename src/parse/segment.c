/**
 * @file segment.c
 * @brief Source segmentation: code versus preprocessor lines.
 */

#include "parse/segment.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <stdlib.h>
#include <string.h>

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

	/*
	 * Walk the source line by line.  Runs of ordinary lines are
	 * accumulated in @cur and flushed as a single code segment whenever
	 * a preprocessor line is met, so the caller sees the original
	 * interleaving of code and directives.
	 */
	StrBuf cur;

	sb_init(&cur);
	const char *p = src;

	while (1) {
		/* Delimit the current line; the tail without '\n' counts too.
		 */
		const char *nl = strchr(p, '\n');
		size_t ll = nl ? (size_t)(nl - p) : strlen(p);
		const char *end = p + ll;

		/* A '#' in the first non-blank column starts a directive. */
		const char *q = p;

		while (q != end && (*q == ' ' || *q == '\t' || *q == '\r' ||
				    *q == '\v' || *q == '\f'))
			q++;
		if (q != end && *q == '#') {
			/* End the pending code run before the directive. */
			if (cur.len > 0) {
				segvec_push(&v, 0, cur.data);
				sb_init(&cur);
			}

			/* Directives pass through verbatim, newline included.
			 */
			StrBuf pl;

			sb_init(&pl);
			sb_putn(&pl, p, ll);
			sb_putc(&pl, '\n');
			segvec_push(&v, 1, pl.data);
		} else {
			/* Plain line: grow the current code run. */
			sb_putn(&cur, p, ll);
			sb_putc(&cur, '\n');
		}

		if (!nl)
			break;
		p = nl + 1;
	}

	/* Flush the trailing code run, if any. */
	if (cur.len > 0)
		segvec_push(&v, 0, cur.data);
	else
		free(cur.data);
	return v;
}
