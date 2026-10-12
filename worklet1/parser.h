#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

/* -------------------------------------------------------------------------
 * Function Table & Lookup
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    int         min_args;
    int         max_args;
} FunctionDef;

extern const FunctionDef FUNCTIONS[];
extern const size_t FUNCTION_COUNT;

int func_lookup(const char *name);

/* -------------------------------------------------------------------------
 * Parsed Expression Struct & Parser Entry Point
 * ------------------------------------------------------------------------- */
typedef struct {
    char       *source;
    TokenList   tokens;
    ExprNode   *root;
    ParseError  error;
} ParsedExpression;

void parsed_expression_init(ParsedExpression *pe);
void parsed_expression_free(ParsedExpression *pe);

int  parse_expression_tree(const TokenList *t, ParsedExpression *out);
int  parse_string_to_ast(const char *input, ParsedExpression *out);

#endif /* PARSER_H */

