/**
 * @file parser.h
 * @brief C statement-level parser: tokenizer and AST builder.
 */

/* C statement-level parser: tokenizer, toplevel splitter and AST builder. */

#ifndef CFLATTEN_PARSER_H
#define CFLATTEN_PARSER_H

#include "parse/ast.h"
#include "parse/token.h"
#include "utils/strvec.h"

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

Token *p_peek(Parser *p, size_t k);

const char *p_peekt(Parser *p, size_t k);

Token *p_next(Parser *p);

int p_expect(Parser *p, const char *t);

int parser_is_typedef_name(Parser *p, const char *w);

int looks_like_decl(Parser *p);

TokVec capture_until_semi(Parser *p);

Node *parse_block_contents(Parser *p, int need_braces);

char *parse_paren_inner_str(Parser *p);

Node *parse_if(Parser *p);

Node *parse_while(Parser *p);

Node *parse_for(Parser *p);

Node *parse_do(Parser *p);

Node *parse_switch(Parser *p);

Node *parse_case(Parser *p);

Node *parse_default(Parser *p);

#endif /* CFLATTEN_PARSER_H */
