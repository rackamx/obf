/**
 * @file parser.h
 * @brief C statement-level parser: tokenizer and AST builder.
 */
/* C statement-level parser: tokenizer, toplevel splitter and AST builder. */
#ifndef CFLATTEN_PARSER_H
#define CFLATTEN_PARSER_H

#include <stddef.h>

#include "util.h"

enum {
	TOK_IDENT = 0,
	TOK_KEYWORD,
	TOK_NUMBER,
	TOK_STRING,
	TOK_CHAR,
	TOK_PUNCT
};
typedef struct {
	int kind;
	char *text;
} Token;
typedef struct {
	Token *items;
	size_t len, cap;
} TokVec;
void tv_init(TokVec *v);
void tv_push(TokVec *v, int kind, char *text);
void tv_free(TokVec *v);
char *strip_comments(const char *src);
TokVec tokenize(const char *code);
char *toks_to_str(Token *toks, size_t n);
char *tokvec_to_str(TokVec *v);

typedef struct {
	int is_preproc;
	char *text;
} Seg;
typedef struct {
	Seg *items;
	size_t len, cap;
} SegVec;
SegVec split_preproc(const char *src);

enum { TL_FUNC = 0, TL_OTHER };
typedef struct {
	int kind;
	char *text;    /* for OTHER */
	TokVec header; /* for FUNC */
	TokVec body;   /* for FUNC incl braces */
	char *name;
} TLItem;
typedef struct {
	TLItem *items;
	size_t len, cap;
} TLVect;
TLVect extract_toplevel(const char *code);

enum {
	N_BLOCK = 0,
	N_EMPTY,
	N_EXPR,
	N_IF,
	N_WHILE,
	N_FOR,
	N_DOWHILE,
	N_SWITCH,
	N_CASE,
	N_DEFAULT,
	N_BREAK,
	N_CONTINUE,
	N_GOTO,
	N_LABEL,
	N_LABELED,
	N_RETURN,
	N_DECL,
	N_DECL_NOVAR,
	N_TYPEDEF
};
typedef struct DeclEnt {
	char *name;
	char *left;
	char *init;
	char *suffix;
	char *stars;
	char *raw;
} DeclEnt;
typedef struct {
	DeclEnt *items;
	size_t len, cap;
} DeclVec;
typedef struct Node Node;
typedef struct {
	Node **items;
	size_t len, cap;
} NodeVec;
struct Node {
	int type;
	NodeVec stmts;	       /* BLOCK */
	char *cond;	       /* IF cond */
	Node *then_b, *else_b; /* IF */
	char *wcond;
	Node *body; /* WHILE/DOWHILE */
	char *init_decl, *init_expr, *for_cond, *incr;
	Node *for_body; /* FOR */
	char *sw_expr;
	Node *sw_body;	 /* SWITCH */
	char *case_expr; /* CASE */
	char *label;
	Node *labeled_stmt; /* GOTO/LABEL/LABELED */
	char *ret_expr;	    /* RETURN (NULL=>bare) */
	char *expr_text;    /* EXPR */
	char *type_str;
	DeclVec decls;
	char *decl_text; /* DECL / NOVAR / TYPEDEF */
};
Node *node_new(int t);
void nv_init(NodeVec *v);
void nv_push(NodeVec *v, Node *n);
void dv_push(DeclVec *v, DeclEnt e);

typedef struct {
	Token *toks;
	size_t n, pos;
	StrVec *typedefs;
} Parser;
int p_eof(Parser *p);
Node *parse_statement(Parser *p);
Node *parse_declaration(Parser *p);
int is_keyword(const char *w);
int is_ident_start(char c);
int is_ident_char(char c);

#endif /* CFLATTEN_PARSER_H */
