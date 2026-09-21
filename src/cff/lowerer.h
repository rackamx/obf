/**
 * @file lowerer.h
 * @brief Lowering of AST statements to basic blocks.
 */

#ifndef CFLATTEN_LOWERER_H
#define CFLATTEN_LOWERER_H

#include "cff/hoister.h"
#include "parse/ast.h"
#include "utils/strvec.h"

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

void lower_init(Lowerer *L, Hoister *h);

int lower_new_block(Lowerer *L);

void lower_emit(Lowerer *L, const char *code);

int lower_cur_term(Lowerer *L);

void loop_push(Lowerer *L, int hb, int b, int hc, int c);

void loop_pop(Lowerer *L);

int find_label(Lowerer *L, const char *n);

void set_label(Lowerer *L, const char *n, int bid);

void casevec_push(CaseVec *v, char *e, int isd, int b);

void sw_walk(Lowerer *L, Node *nd, CaseVec *cases);

void lower_switch(Lowerer *L, Node *nd);

void lower_stmt(Lowerer *L, Node *nd);

int lower_run(Lowerer *L, Node *body);

#endif /* CFLATTEN_LOWERER_H */
