/**
 * @file ast.h
 * @brief Abstract syntax tree nodes and declarator helpers.
 */

#ifndef CFLATTEN_AST_H
#define CFLATTEN_AST_H

#include "parse/token.h"
#include <stddef.h>

/**
 * @brief AST node types.
 */
enum {
	N_BLOCK = 0,  /**< Statement list. */
	N_EMPTY,      /**< Empty statement. */
	N_EXPR,	      /**< Expression statement. */
	N_IF,	      /**< If/else branch. */
	N_WHILE,      /**< While loop. */
	N_FOR,	      /**< For loop. */
	N_DOWHILE,    /**< Do/while loop. */
	N_SWITCH,     /**< Switch dispatch. */
	N_CASE,	      /**< Case label marker. */
	N_DEFAULT,    /**< Default label marker. */
	N_BREAK,      /**< Break. */
	N_CONTINUE,   /**< Continue. */
	N_GOTO,	      /**< Goto. */
	N_LABEL,      /**< Bare user label. */
	N_LABELED,    /**< Labelled statement. */
	N_RETURN,     /**< Return. */
	N_DECL,	      /**< Variable declaration. */
	N_DECL_NOVAR, /**< Declaration without declarator. */
	N_TYPEDEF     /**< Typedef declaration. */
};

/**
 * @brief Single declarator.
 */
typedef struct DeclEnt {
	char *name;   /**< Declared name, or NULL for prototypes. */
	char *left;   /**< Declarator without initializer. */
	char *init;   /**< Initializer text, or NULL. */
	char *suffix; /**< Array suffix such as [10]. */
	char *stars;  /**< Pointer stars such as *. */
	char *raw;    /**< Full declarator text. */
} DeclEnt;

/**
 * @brief Declarator vector.
 */
typedef struct {
	DeclEnt *items; /**< Entries. */
	size_t len;	/**< Item count. */
	size_t cap;	/**< Allocated slots. */
} DeclVec;

typedef struct Node Node;

/**
 * @brief AST node vector.
 */
typedef struct {
	Node **items; /**< Nodes. */
	size_t len;   /**< Item count. */
	size_t cap;   /**< Allocated slots. */
} NodeVec;

/**
 * @brief Statement AST node.
 */
struct Node {
	int type;	    /**< Node type (N_*). */
	NodeVec stmts;	    /**< Block members (N_BLOCK). */
	char *cond;	    /**< If condition (N_IF). */
	Node *then_b;	    /**< Then branch (N_IF). */
	Node *else_b;	    /**< Else branch, or NULL (N_IF). */
	char *wcond;	    /**< Loop condition (N_WHILE/N_DOWHILE). */
	Node *body;	    /**< Loop body (N_WHILE/N_DOWHILE). */
	char *init_decl;    /**< For init declaration, or NULL (N_FOR). */
	char *init_expr;    /**< For init expression, or NULL (N_FOR). */
	char *for_cond;	    /**< For condition, maybe empty (N_FOR). */
	char *incr;	    /**< For increment, maybe empty (N_FOR). */
	Node *for_body;	    /**< For body (N_FOR). */
	char *sw_expr;	    /**< Switch scrutinee (N_SWITCH). */
	Node *sw_body;	    /**< Switch body (N_SWITCH). */
	char *case_expr;    /**< Case value (N_CASE). */
	char *label;	    /**< Label name (N_GOTO/N_LABEL/N_LABELED). */
	Node *labeled_stmt; /**< Labelled statement (N_LABELED). */
	char *ret_expr;	    /**< Return value, or NULL (N_RETURN). */
	char *expr_text;    /**< Statement text (N_EXPR). */
	char *type_str;	    /**< Base type (N_DECL). */
	DeclVec decls;	    /**< Declarators (N_DECL). */
	char *decl_text; /**< Original text (N_DECL/N_DECL_NOVAR/N_TYPEDEF). */
};

Node *node_new(int t);

void nv_init(NodeVec *v);

void nv_push(NodeVec *v, Node *n);

void dv_push(DeclVec *v, DeclEnt e);

const char *extract_decl_name(Token *toks, size_t n);

char *extract_decl_suffix(Token *toks, size_t n, const char *name);

char *extract_decl_stars(Token *toks, size_t n, const char *name);

#endif /* CFLATTEN_AST_H */
