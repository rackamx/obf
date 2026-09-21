/**
 * @file emit.h
 * @brief Dispatcher emission with delta state transitions.
 */

#ifndef CFLATTEN_EMIT_H
#define CFLATTEN_EMIT_H

#include "cff/hoister.h"
#include "cff/lowerer.h"
#include "parse/token.h"

char *sanitize_state(Hoister *h);

char *emit_function(Token *hdr, size_t hn, Hoister *h, Lowerer *L,
		    const char *state);

#endif /* CFLATTEN_EMIT_H */
