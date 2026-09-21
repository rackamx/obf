/**
 * @file token.c
 * @brief C tokenizer: lexical scanner and token vectors.
 */

#include "parse/token.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Plain linear tables: tiny enough that a hash buys nothing. */
static const char *KEYWORDS[] = {
	"auto",	      "break",	   "case",	     "char",
	"const",      "continue",  "default",	     "do",
	"double",     "else",	   "enum",	     "extern",
	"float",      "for",	   "goto",	     "if",
	"inline",     "int",	   "long",	     "register",
	"restrict",   "return",	   "short",	     "signed",
	"sizeof",     "static",	   "struct",	     "switch",
	"typedef",    "union",	   "unsigned",	     "void",
	"volatile",   "while",	   "_Bool",	     "_Complex",
	"_Imaginary", "_Alignas",  "_Alignof",	     "_Atomic",
	"_Generic",   "_Noreturn", "_Static_assert", "_Thread_local",
	NULL};

/* Subset used to recognise declarations; typedef names are tracked
 * separately in the parser. */
static const char *TYPE_KWS[] = {
	"void",	    "char",	 "short",      "int",	   "long",
	"float",    "double",	 "signed",     "unsigned", "struct",
	"union",    "enum",	 "const",      "volatile", "static",
	"extern",   "register",	 "auto",       "inline",   "restrict",
	"_Bool",    "_Complex",	 "_Imaginary", "_Atomic",  "_Thread_local",
	"_Alignas", "typeof",	 "__typeof__", "__typeof", "size_t",
	"ssize_t",  "_Noreturn", NULL};

/**
 * @brief Test whether a word is a C keyword.
 *
 * @param w Word to test.
 *
 * @return 1 for keywords, 0 otherwise.
 */
int is_keyword(const char *w)
{
	for (int i = 0; KEYWORDS[i]; i++)
		if (strcmp(KEYWORDS[i], w) == 0)
			return 1;
	return 0;
}

/**
 * @brief Test whether a word may open a declaration.
 *
 * @param w Word to test.
 *
 * @return 1 for type/qualifier keywords.
 */
int is_type_kw(const char *w)
{
	for (int i = 0; TYPE_KWS[i]; i++)
		if (strcmp(TYPE_KWS[i], w) == 0)
			return 1;
	return 0;
}

/**
 * @brief Initialise an empty token vector.
 *
 * @param v Vector to initialise.
 */
void tv_init(TokVec *v)
{
	v->items = NULL;
	v->len = 0;
	v->cap = 0;
}

/**
 * @brief Append a token, taking ownership of the text.
 *
 * @param v Vector.
 * @param kind Token kind.
 * @param text Token text now owned by the vector.
 */
void tv_push(TokVec *v, int kind, char *text)
{
	if (v->len == v->cap) {
		size_t nc = v->cap ? v->cap * 2 : 64;

		v->items = (Token *)xrealloc(v->items, nc * sizeof(Token));
		v->cap = nc;
	}

	v->items[v->len].kind = kind;
	v->items[v->len].text = text;
	v->len++;
}

/**
 * @brief Release a token vector and all its texts.
 *
 * @param v Vector to release.
 */
void tv_free(TokVec *v)
{
	for (size_t i = 0; i < v->len; i++)
		free(v->items[i].text);
	free(v->items);
	v->items = NULL;
	v->len = v->cap = 0;
}

/**
 * @brief Blank out comments, preserving strings and length.
 *
 * @param src Source text.
 *
 * @return New string of the same length.
 */
char *strip_comments(const char *src)
{
	size_t n = strlen(src);
	char *out = (char *)xmalloc(n + 1);
	size_t i = 0;
	int in_s = 0, in_c = 0, esc = 0;

	/* One pass tracking string/char literal state, so comment markers
	 * inside literals (e.g. "//" in "a//b") are left alone.  Comments
	 * become blanks to keep every offset stable for later stages. */
	while (i < n) {
		char ch = src[i];

		if (in_s) {
			out[i] = ch;
			if (esc)
				esc = 0;
			else if (ch == '\\')
				esc = 1;
			else if (ch == '"')
				in_s = 0;
			i++;
		} else if (in_c) {
			out[i] = ch;
			if (esc)
				esc = 0;
			else if (ch == '\\')
				esc = 1;
			else if (ch == '\'')
				in_c = 0;
			i++;
		} else {
			if (ch == '"') {
				in_s = 1;
				out[i] = ch;
				i++;
			} else if (ch == '\'') {
				in_c = 1;
				out[i] = ch;
				i++;
			} else if (ch == '/' && i + 1 < n &&

				   src[i + 1] == '/') {
				/* Line comment: blank to end of line, keeping
				 * the newline itself so line structure
				 * survives. */
				while (i < n && src[i] != '\n') {
					out[i] = (src[i] == '\n' ||
						  src[i] == '\r')
							 ? src[i]
							 : ' ';
					i++;
				}
			} else if (ch == '/' && i + 1 < n &&

				   src[i + 1] == '*') {
				/* Block comment: blank the opener ... */
				out[i] = ' ';
				out[i + 1] = ' ';
				i += 2;
				/* ... then everything up to the closer,
				 * preserving newlines. */
				while (i < n && !(src[i] == '*' && i + 1 < n &&
						  src[i + 1] == '/')) {
					out[i] = (src[i] == '\n' ||
						  src[i] == '\r')
							 ? src[i]
							 : ' ';
					i++;
				}

				if (i < n) {
					out[i] = ' ';
					if (i + 1 < n)
						out[i + 1] = ' ';
					i += 2;
				}
			} else {
				/* Ordinary character: copy through. */
				out[i] = ch;
				i++;
			}
		}
	}

	out[n] = '\0';
	return out;
}

/**
 * @brief Test for identifier-first characters.
 *
 * @param c Character to test.
 *
 * @return 1 when valid, 0 otherwise.
 */
int is_ident_start(char c)
{
	return isalpha((unsigned char)c) || c == '_' || c == '$';
}

/**
 * @brief Test for identifier characters.
 *
 * @param c Character to test.
 *
 * @return 1 when valid, 0 otherwise.
 */
int is_ident_char(char c)
{
	return isalnum((unsigned char)c) || c == '_' || c == '$';
}

/**
 * @brief Split C source into a token stream.
 *
 * @param code Source text to scan.
 *
 * @return Token vector; every text is newly allocated.
 */
TokVec tokenize(const char *code)
{
	TokVec v;

	tv_init(&v);
	size_t n = strlen(code), i = 0;

	/* Longest-match punctuators first, so ">>=" wins over ">>". */
	static const char *MULTI[] = {
		"...", "<<=", ">>=", "->", "++", "--", "<<", ">>", "<=",
		">=",  "==",  "!=",  "&&", "||", "+=", "-=", "*=", "/=",
		"%=",  "&=",  "|=",  "^=", "##", "::", NULL};
	while (i < n) {
		char c = code[i];

		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
		    c == '\v' || c == '\f') {
			/* Whitespace only separates tokens; skip it. */
			i++;
			continue;
		}

		if (c == '"') {
			/* String literal: scan for the closing quote,
			 * honouring backslash escapes. */
			size_t j = i + 1;
			int esc = 0;

			while (j < n) {
				if (esc)
					esc = 0;
				else if (code[j] == '\\')
					esc = 1;
				else if (code[j] == '"')
					break;
				j++;
			}

			if (j < n)
				j++;
			tv_push(&v, TOK_STRING, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		if (c == '\'') {
			/* Character literal, same idea; a raw newline ends
			 * it early so one bad literal can't eat the file. */
			size_t j = i + 1;
			int esc = 0;

			while (j < n) {
				if (esc)
					esc = 0;
				else if (code[j] == '\\')
					esc = 1;
				else if (code[j] == '\'')
					break;
				else if (code[j] == '\n')
					break;
				j++;
			}

			if (j < n && code[j] == '\'')
				j++;
			tv_push(&v, TOK_CHAR, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		if (is_ident_start(c)) {
			size_t j = i + 1;

			while (j < n && is_ident_char(code[j]))
				j++;
			char *w = xstrndup(code + i, j - i);

			/* Keywords and identifiers share spelling; split them.
			 */
			tv_push(&v, is_keyword(w) ? TOK_KEYWORD : TOK_IDENT, w);
			i = j;
			continue;
		}

		if (isdigit((unsigned char)c) ||
		    (c == '.' && i + 1 < n &&
		     isdigit((unsigned char)code[i + 1]))) {
			/* Deliberately crude: consume the whole run including
			 * hex/float adornments and suffixes as one token. */
			size_t j = i;

			while (j < n && (isalnum((unsigned char)code[j]) ||
					 code[j] == '_' || code[j] == '.' ||
					 code[j] == '\''))
				j++;
			tv_push(&v, TOK_NUMBER, xstrndup(code + i, j - i));
			i = j;
			continue;
		}

		int matched = 0;

		/* Try the multi-character operators before giving up. */
		for (int k = 0; MULTI[k]; k++) {
			size_t L = strlen(MULTI[k]);

			if (i + L <= n && memcmp(code + i, MULTI[k], L) == 0) {
				tv_push(&v, TOK_PUNCT, xstrdup(MULTI[k]));
				i += L;
				matched = 1;
				break;
			}
		}

		if (matched)
			continue;
		/* Anything left is a single-character punctuator. */
		tv_push(&v, TOK_PUNCT, xstrndup(code + i, 1));
		i++;
	}

	return v;
}

/**
 * @brief Join tokens with single spaces.
 *
 * @param toks Tokens.
 * @param n Token count.
 *
 * @return New string.
 */
char *toks_to_str(Token *toks, size_t n)
{
	StrBuf b;

	sb_init(&b);
	for (size_t i = 0; i < n; i++) {
		if (i)
			sb_putc(&b, ' ');
		sb_puts(&b, toks[i].text);
	}

	return b.data;
}

/**
 * @brief Join a token vector with single spaces.
 *
 * @param v Tokens.
 *
 * @return New string.
 */
char *tokvec_to_str(TokVec *v)
{
	return toks_to_str(v->items, v->len);
}
