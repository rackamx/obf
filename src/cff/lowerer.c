/**
 * @file lowerer.c
 * @brief Lowering of AST statements to basic blocks.
 */

#include "cff/lowerer.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lower_stmt(Lowerer *L, Node *nd);

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

	size_t n = strlen(code);

	while (n && isspace((unsigned char)code[n - 1]))
		n--;
	char *s = xstrndup(code, n);

	/* Array-init snippets end with '}', plain statements need ';'. */
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

	/* Like lower_stmt, but case/default markers anywhere in the nest
	 * open dispatch target blocks instead of falling through. */
	if (tp == N_BLOCK) {
		for (size_t i = 0; i < nd->stmts.len; i++)
			sw_walk(L, nd->stmts.items[i], cases);
	} else if (tp == N_CASE) {
		int nb = lower_new_block(L);

		/* Fallthrough from the previous case body, if it is still
		 * open; a terminated one (break/return) stays that way. */
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

	/* Evaluate the scrutinee once into a typed temporary; repeating
	 * the expression per case would redo side effects. */
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
	/* A break here must leave the switch, so only a break target is
	 * pushed; continue still belongs to any enclosing loop. */
	loop_push(L, 1, end_b, 0, 0);
	sw_walk(L, nd->sw_body, &cases);
	if (!lower_cur_term(L)) {
		L->blocks.items[L->cur].term = TERM_GOTO;
		L->blocks.items[L->cur].target = end_b;
		L->cur = lower_new_block(L);
	}

	/* An unmatched value falls to default, else past the switch. */
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
		/* Empty switch body: straight to the end. */
		L->blocks.items[disp_b].term = TERM_GOTO;
		L->blocks.items[disp_b].target = end_b;
	} else if (nn == 0) {
		/* Default only: no tests needed. */
		L->blocks.items[disp_b].term = TERM_GOTO;
		L->blocks.items[disp_b].target = def_tgt;
	} else {
		int cur_test = disp_b;

		/* Chain "tmp == value ? case : next-test" blocks, ending
		 * at the default target. */
		for (size_t k = 0; k < nn; k++) {
			int ci = nondef_idx[k];
			int cblk = cases.items[ci].blk;
			char *cexpr = cases.items[ci].expr;
			int next_test = -1;

			if (k + 1 < nn) {
				next_test = lower_new_block(L);
			}

			int false_tgt = (next_test >= 0) ? next_test : def_tgt;

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
		/* Without an else the false edge skips both branches. */
		cb->target2 = (nd->else_b ? else_b : end_b);
		L->cur = then_b;

		lower_stmt(L, nd->then_b);
		if (!lower_cur_term(L)) {
			/* Open fallthrough: rejoin at the end.  A terminated
			 * branch (return/goto) already left dead code behind
			 * and needs no edge. */
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
			/* No else to lower: forward the empty block. */
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

		/* Break exits, continue retests. */
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
		/* Declaration inits were already hoisted to assignments by
		 * the hoister; only expression inits run here. */
		if (nd->init_expr && nd->init_expr[0]) {
			const char *p = nd->init_expr;

			while (*p && isspace((unsigned char)*p))
				p++;
			if (*p) {
				StrBuf b;

				sb_init(&b);
				sb_puts(&b, nd->init_expr);

				/* The init text may lack its ';': append one.
				 */
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

		/* Continue runs the increment first, unlike while. */
		loop_push(L, 1, end_b, 1, incr_b);
		lower_stmt(L, nd->for_body);
		loop_pop(L);
		if (!lower_cur_term(L)) {
			L->blocks.items[L->cur].term = TERM_GOTO;
			L->blocks.items[L->cur].target = incr_b;
			L->cur = lower_new_block(L);
		}

		/* The increment is its own block so continue lands on it. */
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
				/* The increment runs on entry to its own block,
				 * unlike ordinary straight-line code. */
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

		/* Body first: do/while always runs once. */
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

		/* Innermost enclosing loop or switch wins, so a break
		 * inside a switch in a loop leaves the switch. */
		for (size_t i = L->llen; i > 0; i--)
			if (L->loops[i - 1].has_break) {
				tgt = L->loops[i - 1].brk;
				break;
			}

		if (tgt < 0) {
			/* Stray break outside anything: end the function. */
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
		/* Target -2 marks "label, to resolve": forward labels may
		 * not exist yet when the goto is lowered. */
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
		/* Declarations never reach lowering; the hoister consumed
		 * them into assignments and hoisted text. */
	}
}

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

	/* Any block left open (e.g. dead code after a return) exits. */
	for (size_t i = 0; i < L->blocks.len; i++)
		if (L->blocks.items[i].term == TERM_NONE)
			L->blocks.items[i].term = TERM_EXIT;

	/* Forward gotos are lowered before their label exists, so patch
	 * every pending "-2" target now.  Unknown labels get a dead end
	 * rather than dangling. */
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

	/* Drop whatever the entry cannot reach: dead ends from returns
	 * and breaks only bloat the dispatcher. */
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

	/* Renumber contiguously with the entry first, so case labels read
	 * 0..N-1 and the entry is always case 0. */
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

		/* Labels are debug-only past lowering; remap them through
		 * the same order table. */
		if (old >= 0 && (size_t)old < on * 2) {
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
