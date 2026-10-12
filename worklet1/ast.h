#ifndef AST_H
#define AST_H

#include <stddef.h>
#include "decimal.h"
#include "complex.h"

typedef enum {
    NODE_NUMBER,
    NODE_VAR,
    NODE_UNARY,
    NODE_BINARY,
    NODE_CALL
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_NEG, OP_POS, OP_FACT
} OpCode;

typedef enum {
    VAL_INT,
    VAL_DEC,
    VAL_COMPLEX
} ValueType;

typedef struct {
    ValueType type;
    union {
        BigInteger i;
        BigDecimal d;
        Complex    c;
    } as;
} Value;

typedef struct ExprNode {
    NodeType type;
    size_t   pos; /* 1-indexed source column */
    union {
        Value   number;
        char   *var_name;
        struct {
            OpCode           op;
            struct ExprNode *left;
            struct ExprNode *right;
        } bin;
        struct {
            OpCode           op;
            struct ExprNode *operand;
        } un;
        struct {
            int              fn_id;
            size_t           argc;
            struct ExprNode **args;
        } call;
    } as;
} ExprNode;

/* -------------------------------------------------------------------------
 * AST Node Constructors & Destructor
 * ------------------------------------------------------------------------- */
ExprNode *node_new_number(const char *str, size_t pos);
ExprNode *node_new_var(const char *name, size_t pos);
ExprNode *node_new_binary(OpCode op, ExprNode *left, ExprNode *right, size_t pos);
ExprNode *node_new_unary(OpCode op, ExprNode *operand, size_t pos);
ExprNode *node_new_call(int fn_id, ExprNode **args, size_t argc, size_t pos);
void      free_expr_node(ExprNode *root);

#endif /* AST_H */

