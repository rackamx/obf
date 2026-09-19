/**
 * @file flatten.h
 * @brief Control-flow flattening: hoisting, lowering, emission.
 */
/* Control-flow flattening: declaration hoisting, basic-block lowering and
 * dispatcher emission.
 */
#ifndef CFLATTEN_FLATTEN_H
#define CFLATTEN_FLATTEN_H
#include "parser.h"
char *flatten_function(TokVec *header, TokVec *body, StrVec *global_td);

char *flatten_program(const char *src);
#endif /* CFLATTEN_FLATTEN_H */
