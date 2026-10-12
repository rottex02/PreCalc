#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

const char *token_type_name(TokenType type) {
    switch (type) {
        case TK_NUMBER: return "NUMBER";
        case TK_IDENT:  return "IDENT";
        case TK_OP:     return "OP";
        case TK_LPAREN: return "LPAREN";
        case TK_RPAREN: return "RPAREN";
        case TK_COMMA:  return "COMMA";
        case TK_END:    return "END";
        default:        return "UNKNOWN";
    }
}

/* -------------------------------------------------------------------------
 * Token List Lifecycle
 * ------------------------------------------------------------------------- */

void token_list_init(TokenList *tl) {
    if (!tl) return;
    tl->items = NULL;
    tl->count = 0;
    tl->capacity = 0;
}

void token_list_free(TokenList *tl) {
    if (!tl) return;
    if (tl->items) {
        for (size_t i = 0; i < tl->count; i++) {
            if (tl->items[i].text) {
                free(tl->items[i].text);
                tl->items[i].text = NULL;
            }
        }
        free(tl->items);
        tl->items = NULL;
    }
    tl->count = 0;
    tl->capacity = 0;
}

int token_list_add(TokenList *tl, TokenType type, const char *text, size_t pos) {
    if (!tl) return 0;

    if (tl->count >= tl->capacity) {
        size_t new_cap = (tl->capacity == 0 ? 16 : tl->capacity * 2);
        Token *new_items = (Token *)realloc(tl->items, new_cap * sizeof(Token));
        if (!new_items) return 0;
        tl->items = new_items;
        tl->capacity = new_cap;
    }

    size_t len = (text ? strlen(text) : 0);
    char *dup = (char *)malloc(len + 1);
    if (!dup) return 0;
    if (text) {
        memcpy(dup, text, len);
    }
    dup[len] = '\0';

    tl->items[tl->count].type = type;
    tl->items[tl->count].text = dup;
    tl->items[tl->count].pos = pos;
    tl->count++;
    return 1;
}

/* -------------------------------------------------------------------------
 * Pre-validation
 * ------------------------------------------------------------------------- */

int validate_expression(const char *input, size_t *err_pos) {
    if (err_pos) *err_pos = 0;
    if (!input) return 0;

    /* Check for non-empty input */
    const char *p = input;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) {
        if (err_pos) *err_pos = 1;
        return 0;
    }

    size_t col = 1;
    size_t paren_stack[512];
    size_t paren_depth = 0;

    p = input;
    while (*p) {
        char c = *p;

        if (c == '(') {
            if (paren_depth < sizeof(paren_stack) / sizeof(paren_stack[0])) {
                paren_stack[paren_depth] = col;
            }
            paren_depth++;
        } else if (c == ')') {
            if (paren_depth == 0) {
                if (err_pos) *err_pos = col;
                return 0; /* Unmatched closing parenthesis */
            }
            paren_depth--;
        } else if (!isspace((unsigned char)c) && !isalnum((unsigned char)c) && c != '_') {
            /* Check allowed operators and punctuation */
            if (!strchr("+-*/%^!=;,. ", c)) {
                if (err_pos) *err_pos = col;
                return 0; /* Disallowed character */
            }
        }

        p++;
        col++;
    }

    if (paren_depth > 0) {
        /* Unclosed opening parenthesis */
        if (err_pos) {
            size_t idx = (paren_depth - 1 < sizeof(paren_stack) / sizeof(paren_stack[0]))
                         ? paren_depth - 1 : 0;
            *err_pos = paren_stack[idx];
        }
        return 0;
    }

    return 1;
}

/* -------------------------------------------------------------------------
 * Lexer (Tokenizer)
 * ------------------------------------------------------------------------- */

int lex_tokenize(const char *input, TokenList *out, ParseError *err) {
    if (!out) return 0;
    token_list_init(out);

    if (err) {
        err->code = ERR_OK;
        err->position = 0;
        err->message[0] = '\0';
    }

    if (!input) {
        if (err) {
            err->code = ERR_EMPTY_EXPR;
            err->position = 1;
            snprintf(err->message, sizeof(err->message), "Empty expression");
        }
        return 0;
    }

    const char *p = input;
    size_t col = 1;

    while (*p) {
        /* 1. Skip whitespace */
        if (isspace((unsigned char)*p)) {
            p++;
            col++;
            continue;
        }

        /* 2. Numeric literal (digits, optional dot, optional exponent) */
        if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)*(p + 1)))) {
            size_t start_col = col;
            const char *start_ptr = p;
            int has_dot = 0;

            if (*p == '.') {
                has_dot = 1;
                p++;
                col++;
            }

            while (isdigit((unsigned char)*p)) {
                p++;
                col++;
            }

            if (!has_dot && *p == '.' && isdigit((unsigned char)*(p + 1))) {
                has_dot = 1;
                p++; /* consume '.' */
                col++;
                while (isdigit((unsigned char)*p)) {
                    p++;
                    col++;
                }
            }

            /* Optional scientific notation exponent: [eE][+-]?[0-9]+ */
            if (*p == 'e' || *p == 'E') {
                const char *exp_ptr = p + 1;
                if (*exp_ptr == '+' || *exp_ptr == '-') {
                    exp_ptr++;
                }
                if (isdigit((unsigned char)*exp_ptr)) {
                    p = exp_ptr;
                    col = start_col + (size_t)(p - start_ptr);
                    while (isdigit((unsigned char)*p)) {
                        p++;
                        col++;
                    }
                }
            }

            size_t len = (size_t)(p - start_ptr);
            char *num_str = (char *)malloc(len + 1);
            if (!num_str) {
                token_list_free(out);
                return 0;
            }
            memcpy(num_str, start_ptr, len);
            num_str[len] = '\0';

            if (!token_list_add(out, TK_NUMBER, num_str, start_col)) {
                free(num_str);
                token_list_free(out);
                return 0;
            }
            free(num_str);
            continue;
        }

        /* 3. Identifiers: [a-zA-Z_][a-zA-Z0-9_]* */
        if (isalpha((unsigned char)*p) || *p == '_') {
            size_t start_col = col;
            const char *start_ptr = p;

            while (isalnum((unsigned char)*p) || *p == '_') {
                p++;
                col++;
            }

            size_t len = (size_t)(p - start_ptr);
            char *ident_str = (char *)malloc(len + 1);
            if (!ident_str) {
                token_list_free(out);
                return 0;
            }
            memcpy(ident_str, start_ptr, len);
            ident_str[len] = '\0';

            if (!token_list_add(out, TK_IDENT, ident_str, start_col)) {
                free(ident_str);
                token_list_free(out);
                return 0;
            }
            free(ident_str);
            continue;
        }

        /* 4. Grouping & Delimiters */
        if (*p == '(') {
            if (!token_list_add(out, TK_LPAREN, "(", col)) {
                token_list_free(out);
                return 0;
            }
            p++;
            col++;
            continue;
        }

        if (*p == ')') {
            if (!token_list_add(out, TK_RPAREN, ")", col)) {
                token_list_free(out);
                return 0;
            }
            p++;
            col++;
            continue;
        }

        if (*p == ',') {
            if (!token_list_add(out, TK_COMMA, ",", col)) {
                token_list_free(out);
                return 0;
            }
            p++;
            col++;
            continue;
        }

        /* 5. Operators: +, -, *, /, %, ^, ! */
        if (strchr("+-*/%^!", *p)) {
            char op_str[2] = { *p, '\0' };
            if (!token_list_add(out, TK_OP, op_str, col)) {
                token_list_free(out);
                return 0;
            }
            p++;
            col++;
            continue;
        }

        /* 6. Disallowed / Illegal Character */
        if (err) {
            err->code = ERR_INVALID_CHAR;
            err->position = col;
            snprintf(err->message, sizeof(err->message),
                     "Illegal character '%c' at column %zu", *p, col);
        }
        token_list_free(out);
        return 0;
    }

    /* Append TK_END sentinel */
    if (!token_list_add(out, TK_END, "", col)) {
        token_list_free(out);
        return 0;
    }

    return 1;
}

