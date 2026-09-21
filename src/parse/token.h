/**
 * @file token.h
 * @brief C tokenizer: lexical scanner and token vectors.
 */

#ifndef CFLATTEN_TOKEN_H
#define CFLATTEN_TOKEN_H

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

int is_keyword(const char *w);

int is_type_kw(const char *w);

void tv_init(TokVec *v);

void tv_push(TokVec *v, int kind, char *text);

void tv_free(TokVec *v);

char *strip_comments(const char *src);

int is_ident_start(char c);

int is_ident_char(char c);

TokVec tokenize(const char *code);

char *toks_to_str(Token *toks, size_t n);

char *tokvec_to_str(TokVec *v);

#endif /* CFLATTEN_TOKEN_H */
