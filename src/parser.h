/**
 * @file parser.h
 * @brief C statement-level parser: tokenizer and AST builder.
 */
/* C statement-level parser: tokenizer, toplevel splitter and AST builder. */
#ifndef CFLATTEN_PARSER_H
#define CFLATTEN_PARSER_H
#include "util.h"
#include <stddef.h>
/**
 * @brief Token kinds.
 */
enum {
	TOK_IDENT = 0, /**< Identifier. */
	TOK_KEYWORD,   /**< C keyword. */
	TOK_NUMBER,    /**< Numeric literal. */
	TOK_STRING,    /**< String literal. */
	TOK_CHAR,      /**< Character literal. */
	TOK_PUNCT      /**< Operator or punctuation. */
};

/**
 * @brief Lexical token.
 */
typedef struct {
	int kind;   /**< Kind (TOK_*). */
	char *text; /**< Spelling (owned). */
} Token;

/**
 * @brief Growable token vector.
 */
typedef struct {
	Token *items; /**< Tokens. */
	size_t len;   /**< Item count. */
	size_t cap;   /**< Allocated slots. */
} TokVec;

void tv_init(TokVec *v);

void tv_push(TokVec *v, int kind, char *text);

void tv_free(TokVec *v);

char *strip_comments(const char *src);

TokVec tokenize(const char *code);

char *toks_to_str(Token *toks, size_t n);

char *tokvec_to_str(TokVec *v);

/**
 * @brief Source segment.
 */
typedef struct {
	int is_preproc; /**< Non-zero for preprocessor lines. */
	char *text;	/**< Segment text with newlines (owned). */
} Seg;

/**
 * @brief Source segment vector.
 */
typedef struct {
	Seg *items; /**< Segments. */
	size_t len; /**< Item count. */
	size_t cap; /**< Allocated slots. */
} SegVec;

SegVec split_preproc(const char *src);

/**
 * @brief Toplevel declaration kinds.
 */
enum {
	TL_FUNC = 0, /**< Function definition. */
	TL_OTHER     /**< Any other toplevel text. */
};

/**
 * @brief Toplevel declaration.
 */
typedef struct {
	int kind;      /**< TL_FUNC or TL_OTHER. */
	char *text;    /**< Other text (NULL for functions). */
	TokVec header; /**< Function header tokens. */
	TokVec body;   /**< Braced body tokens. */
	char *name;    /**< Function name. */
} TLItem;

/**
 * @brief Toplevel item vector.
 */
typedef struct {
	TLItem *items; /**< Items. */
	size_t len;    /**< Item count. */
	size_t cap;    /**< Allocated slots. */
} TLVect;

TLVect extract_toplevel(const char *code);

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

struct Node {
	int type;
	NodeVec stmts; /* BLOCK */
	char *cond;    /* IF cond */
	Node *then_b;
	Node *else_b; /* IF */
	char *wcond;
	Node *body; /* WHILE/DOWHILE */
	char *init_decl;
	char *init_expr;
	char *for_cond;
	char *incr;
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

/**
 * @brief Token cursor with typedef names.
 */
typedef struct {
	Token *toks;	  /**< Input tokens (borrowed). */
	size_t n;	  /**< Token count. */
	size_t pos;	  /**< Cursor. */
	StrVec *typedefs; /**< Known typedef names. */
} Parser;

int p_eof(Parser *p);

Node *parse_statement(Parser *p);

Node *parse_declaration(Parser *p);

int is_keyword(const char *w);

int is_ident_start(char c);

int is_ident_char(char c);
#endif /* CFLATTEN_PARSER_H */
