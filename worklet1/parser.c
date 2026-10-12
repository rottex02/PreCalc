#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const FunctionDef FUNCTIONS[] = {
    { "sin",  1, 1 },
    { "cos",  1, 1 },
    { "exp",  1, 1 },
    { "ln",   1, 1 },
    { "sqrt", 1, 1 },
    { "abs",  1, 1 },
    { "gcd",  2, 2 },
    { "pow",  2, 2 },
    { "mod",  2, 2 },
    { "fact", 1, 1 }
};
const size_t FUNCTION_COUNT = sizeof(FUNCTIONS) / sizeof(FUNCTIONS[0]);

int func_lookup(const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < FUNCTION_COUNT; i++) {
        if (strcmp(name, FUNCTIONS[i].name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* -------------------------------------------------------------------------
 * Parser Context & Forward Declarations
 * ------------------------------------------------------------------------- */
typedef struct {
    const TokenList *tokens;
    size_t           cursor;
    ParseError      *error;
} Parser;

static ExprNode *parse_expression(Parser *p);
static ExprNode *parse_term(Parser *p);
static ExprNode *parse_power(Parser *p);
static ExprNode *parse_unary(Parser *p);
static ExprNode *parse_postfix(Parser *p);
static ExprNode *parse_primary(Parser *p);

static const Token *current_token(Parser *p) {
    if (p->cursor < p->tokens->count) {
        return &p->tokens->items[p->cursor];
    }
    /* Fallback to last token (sentinel) */
    return &p->tokens->items[p->tokens->count - 1];
}

static const Token *peek_next_token(Parser *p) {
    if (p->cursor + 1 < p->tokens->count) {
        return &p->tokens->items[p->cursor + 1];
    }
    return NULL;
}

static void advance(Parser *p) {
    if (p->cursor < p->tokens->count) {
        p->cursor++;
    }
}

static int match_op(Parser *p, const char *op_str) {
    const Token *t = current_token(p);
    return (t->type == TK_OP && strcmp(t->text, op_str) == 0);
}

static int match_type(Parser *p, TokenType type) {
    return (current_token(p)->type == type);
}

static void set_error(Parser *p, int code, size_t pos, const char *msg) {
    if (p->error) {
        p->error->code = code;
        p->error->position = pos;
        snprintf(p->error->message, sizeof(p->error->message), "%s", msg);
    }
}

/* -------------------------------------------------------------------------
 * Recursive-Descent Grammar Implementation
 * ------------------------------------------------------------------------- */

/* expression -> term (( '+' | '-' ) term)* */
static ExprNode *parse_expression(Parser *p) {
    ExprNode *left = parse_term(p);
    if (!left) return NULL;

    while (match_op(p, "+") || match_op(p, "-")) {
        OpCode op = (strcmp(current_token(p)->text, "+") == 0 ? OP_ADD : OP_SUB);
        size_t op_pos = current_token(p)->pos;
        advance(p);

        ExprNode *right = parse_term(p);
        if (!right) {
            free_expr_node(left);
            return NULL;
        }

        left = node_new_binary(op, left, right, op_pos);
        if (!left) return NULL;
    }

    return left;
}

/* term -> power (( '*' | '/' | '%' ) power)* */
static ExprNode *parse_term(Parser *p) {
    ExprNode *left = parse_power(p);
    if (!left) return NULL;

    while (match_op(p, "*") || match_op(p, "/") || match_op(p, "%")) {
        OpCode op;
        if (strcmp(current_token(p)->text, "*") == 0) op = OP_MUL;
        else if (strcmp(current_token(p)->text, "/") == 0) op = OP_DIV;
        else op = OP_MOD;

        size_t op_pos = current_token(p)->pos;
        advance(p);

        ExprNode *right = parse_power(p);
        if (!right) {
            free_expr_node(left);
            return NULL;
        }

        left = node_new_binary(op, left, right, op_pos);
        if (!left) return NULL;
    }

    return left;
}

/* power -> unary ( '^' power )?  [Right-associative] */
static ExprNode *parse_power(Parser *p) {
    ExprNode *left = parse_unary(p);
    if (!left) return NULL;

    if (match_op(p, "^")) {
        size_t op_pos = current_token(p)->pos;
        advance(p);

        /* Right-recursive call to parse_power handles right-associativity */
        ExprNode *right = parse_power(p);
        if (!right) {
            free_expr_node(left);
            return NULL;
        }

        return node_new_binary(OP_POW, left, right, op_pos);
    }

    return left;
}

/* unary -> ( '+' | '-' ) unary | postfix  [Positional sign disambiguation] */
static ExprNode *parse_unary(Parser *p) {
    if (match_op(p, "+") || match_op(p, "-")) {
        OpCode op = (strcmp(current_token(p)->text, "+") == 0 ? OP_POS : OP_NEG);
        size_t op_pos = current_token(p)->pos;
        advance(p);

        ExprNode *operand = parse_unary(p);
        if (!operand) return NULL;

        return node_new_unary(op, operand, op_pos);
    }

    return parse_postfix(p);
}

/* postfix -> primary ( '!' )*  [Factorial] */
static ExprNode *parse_postfix(Parser *p) {
    ExprNode *node = parse_primary(p);
    if (!node) return NULL;

    while (match_op(p, "!")) {
        size_t op_pos = current_token(p)->pos;
        advance(p);

        node = node_new_unary(OP_FACT, node, op_pos);
        if (!node) return NULL;
    }

    return node;
}

/* primary -> NUMBER | IDENT | IDENT '(' arglist ')' | '(' expression ')' */
static ExprNode *parse_primary(Parser *p) {
    const Token *tok = current_token(p);

    /* 1. Numeric Literal */
    if (match_type(p, TK_NUMBER)) {
        ExprNode *node = node_new_number(tok->text, tok->pos);
        if (!node) {
            set_error(p, ERR_INVALID_NUMBER, tok->pos, "Failed to parse number literal");
            return NULL;
        }
        advance(p);
        return node;
    }

    /* 2. Identifier (Function Call or Variable) */
    if (match_type(p, TK_IDENT)) {
        char *name = tok->text;
        size_t name_pos = tok->pos;
        const Token *next = peek_next_token(p);

        /* Check if it is followed by '(' -> Function Call */
        if (next && next->type == TK_LPAREN) {
            int fn_id = func_lookup(name);
            if (fn_id < 0) {
                set_error(p, 10, name_pos, "Unknown function name");
                return NULL;
            }

            advance(p); /* consume function name */
            advance(p); /* consume '(' */

            /* Parse comma-separated arglist */
            size_t argc = 0;
            size_t arg_cap = 4;
            ExprNode **args = (ExprNode **)malloc(arg_cap * sizeof(ExprNode *));
            if (!args) return NULL;

            if (!match_type(p, TK_RPAREN)) {
                while (1) {
                    ExprNode *arg = parse_expression(p);
                    if (!arg) {
                        for (size_t i = 0; i < argc; i++) free_expr_node(args[i]);
                        free(args);
                        return NULL;
                    }

                    if (argc >= arg_cap) {
                        arg_cap *= 2;
                        ExprNode **new_args = (ExprNode **)realloc(args, arg_cap * sizeof(ExprNode *));
                        if (!new_args) {
                            for (size_t i = 0; i < argc; i++) free_expr_node(args[i]);
                            free_expr_node(arg);
                            free(args);
                            return NULL;
                        }
                        args = new_args;
                    }
                    args[argc++] = arg;

                    if (match_type(p, TK_COMMA)) {
                        advance(p); /* consume ',' */
                    } else {
                        break;
                    }
                }
            }

            if (!match_type(p, TK_RPAREN)) {
                set_error(p, ERR_UNBALANCED_PAREN, current_token(p)->pos, "Expected ')' closing argument list");
                for (size_t i = 0; i < argc; i++) free_expr_node(args[i]);
                free(args);
                return NULL;
            }
            advance(p); /* consume ')' */

            /* Validate arity at parse time */
            if ((int)argc < FUNCTIONS[fn_id].min_args || (int)argc > FUNCTIONS[fn_id].max_args) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Function '%s' takes %d argument(s), but %zu given",
                         FUNCTIONS[fn_id].name, FUNCTIONS[fn_id].min_args, argc);
                set_error(p, 11, name_pos, msg);
                for (size_t i = 0; i < argc; i++) free_expr_node(args[i]);
                free(args);
                return NULL;
            }

            return node_new_call(fn_id, args, argc, name_pos);
        }

        /* Not a call -> Variable name */
        ExprNode *node = node_new_var(name, name_pos);
        advance(p);
        return node;
    }

    /* 3. Grouped Expression: '(' expression ')' */
    if (match_type(p, TK_LPAREN)) {
        size_t lparen_pos = tok->pos;
        advance(p); /* consume '(' */

        ExprNode *inner = parse_expression(p);
        if (!inner) return NULL;

        if (!match_type(p, TK_RPAREN)) {
            set_error(p, ERR_UNBALANCED_PAREN, lparen_pos, "Unclosed '(' opened at this column");
            free_expr_node(inner);
            return NULL;
        }
        advance(p); /* consume ')' */

        return inner;
    }

    /* 4. Unexpected Token */
    set_error(p, 12, tok->pos, "Unexpected token or missing operand");
    return NULL;
}

/* -------------------------------------------------------------------------
 * Parsed Expression Lifecycle & Entry Point
 * ------------------------------------------------------------------------- */

void parsed_expression_init(ParsedExpression *pe) {
    if (!pe) return;
    pe->source = NULL;
    token_list_init(&pe->tokens);
    pe->root = NULL;
    pe->error.code = ERR_OK;
    pe->error.position = 0;
    pe->error.message[0] = '\0';
}

void parsed_expression_free(ParsedExpression *pe) {
    if (!pe) return;
    if (pe->source) {
        free(pe->source);
        pe->source = NULL;
    }
    token_list_free(&pe->tokens);
    if (pe->root) {
        free_expr_node(pe->root);
        pe->root = NULL;
    }
}

int parse_expression_tree(const TokenList *t, ParsedExpression *out) {
    if (!t || !out) return 0;
    out->root = NULL;

    Parser parser;
    parser.tokens = t;
    parser.cursor = 0;
    parser.error = &out->error;

    if (t->count == 0 || (t->count == 1 && t->items[0].type == TK_END)) {
        set_error(&parser, ERR_EMPTY_EXPR, 1, "Empty expression");
        return 0;
    }

    ExprNode *root = parse_expression(&parser);
    if (!root) {
        return 0;
    }

    /* Check if full token stream was consumed */
    if (current_token(&parser)->type != TK_END) {
        set_error(&parser, 13, current_token(&parser)->pos, "Unexpected extra token after valid expression");
        free_expr_node(root);
        return 0;
    }

    out->root = root;
    return 1;
}

int parse_string_to_ast(const char *input, ParsedExpression *out) {
    if (!out) return 0;
    parsed_expression_init(out);

    if (!input) {
        out->error.code = ERR_EMPTY_EXPR;
        out->error.position = 1;
        snprintf(out->error.message, sizeof(out->error.message), "Empty input");
        return 0;
    }

    out->source = strdup(input);

    /* 1. Pre-validate */
    size_t err_pos = 0;
    if (!validate_expression(input, &err_pos)) {
        out->error.code = ERR_UNBALANCED_PAREN;
        out->error.position = err_pos;
        snprintf(out->error.message, sizeof(out->error.message), "Invalid syntax or unbalanced parenthesis at col %zu", err_pos);
        return 0;
    }

    /* 2. Tokenize */
    if (!lex_tokenize(input, &out->tokens, &out->error)) {
        return 0;
    }

    /* 3. Parse to AST */
    if (!parse_expression_tree(&out->tokens, out)) {
        return 0;
    }

    return 1;
}
