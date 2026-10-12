#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

/* -------------------------------------------------------------------------
 * Token & Lexer Structures
 * ------------------------------------------------------------------------- */

typedef enum {
    TK_NUMBER,   /* e.g. "123", "45.67", "1e-5" */
    TK_IDENT,    /* e.g. "sin", "gcd", "x", "ans" */
    TK_OP,       /* e.g. "+", "-", "*", "/", "%", "^", "!" */
    TK_LPAREN,   /* "(" */
    TK_RPAREN,   /* ")" */
    TK_COMMA,    /* "," */
    TK_END       /* End-of-input sentinel */
} TokenType;

typedef struct {
    TokenType type;
    char     *text;
    size_t    pos; /* 1-indexed column for caret error reporting */
} Token;

typedef struct {
    Token  *items;
    size_t  count;
    size_t  capacity;
} TokenList;

typedef enum {
    ERR_OK = 0,
    ERR_INVALID_CHAR,
    ERR_UNBALANCED_PAREN,
    ERR_INVALID_NUMBER,
    ERR_EMPTY_EXPR
} ErrorCode;

typedef struct {
    int    code;
    size_t position; /* 1-indexed column */
    char   message[256];
} ParseError;

/* -------------------------------------------------------------------------
 * Pre-validation & Token List Lifecycle
 * ------------------------------------------------------------------------- */
int  validate_expression(const char *input, size_t *err_pos);
void token_list_init(TokenList *tl);
void token_list_free(TokenList *tl);
int  token_list_add(TokenList *tl, TokenType type, const char *text, size_t pos);

/* -------------------------------------------------------------------------
 * Lexer Entry Point
 * ------------------------------------------------------------------------- */
int  lex_tokenize(const char *input, TokenList *out, ParseError *err);

/* Helper to convert token type to human readable name */
const char *token_type_name(TokenType type);

#endif /* LEXER_H */

