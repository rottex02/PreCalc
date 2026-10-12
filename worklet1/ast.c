#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include "bigint_io.h"
#include <stdlib.h>
#include <string.h>

ExprNode *node_new_number(const char *str, size_t pos) {
    if (!str) return NULL;
    ExprNode *node = (ExprNode *)calloc(1, sizeof(ExprNode));
    if (!node) return NULL;

    node->type = NODE_NUMBER;
    node->pos = pos;

    /* Check if integer or decimal */
    if (strchr(str, '.') == NULL && strchr(str, 'e') == NULL && strchr(str, 'E') == NULL) {
        node->as.number.type = VAL_INT;
        bigint_init(&node->as.number.as.i);
        if (!bigint_from_string(&node->as.number.as.i, str)) {
            bigint_free(&node->as.number.as.i);
            free(node);
            return NULL;
        }
    } else {
        node->as.number.type = VAL_DEC;
        decimal_init(&node->as.number.as.d);
        if (!decimal_from_string(&node->as.number.as.d, str)) {
            decimal_free(&node->as.number.as.d);
            free(node);
            return NULL;
        }
    }
    return node;
}

ExprNode *node_new_var(const char *name, size_t pos) {
    if (!name) return NULL;
    ExprNode *node = (ExprNode *)calloc(1, sizeof(ExprNode));
    if (!node) return NULL;

    node->type = NODE_VAR;
    node->pos = pos;
    node->as.var_name = strdup(name);
    if (!node->as.var_name) {
        free(node);
        return NULL;
    }
    return node;
}

ExprNode *node_new_binary(OpCode op, ExprNode *left, ExprNode *right, size_t pos) {
    if (!left || !right) return NULL;
    ExprNode *node = (ExprNode *)calloc(1, sizeof(ExprNode));
    if (!node) return NULL;

    node->type = NODE_BINARY;
    node->pos = pos;
    node->as.bin.op = op;
    node->as.bin.left = left;
    node->as.bin.right = right;
    return node;
}

ExprNode *node_new_unary(OpCode op, ExprNode *operand, size_t pos) {
    if (!operand) return NULL;
    ExprNode *node = (ExprNode *)calloc(1, sizeof(ExprNode));
    if (!node) return NULL;

    node->type = NODE_UNARY;
    node->pos = pos;
    node->as.un.op = op;
    node->as.un.operand = operand;
    return node;
}

ExprNode *node_new_call(int fn_id, ExprNode **args, size_t argc, size_t pos) {
    ExprNode *node = (ExprNode *)calloc(1, sizeof(ExprNode));
    if (!node) return NULL;

    node->type = NODE_CALL;
    node->pos = pos;
    node->as.call.fn_id = fn_id;
    node->as.call.argc = argc;
    node->as.call.args = args;
    return node;
}

void free_expr_node(ExprNode *root) {
    if (!root) return;

    switch (root->type) {
        case NODE_NUMBER:
            if (root->as.number.type == VAL_INT) {
                bigint_free(&root->as.number.as.i);
            } else if (root->as.number.type == VAL_DEC) {
                decimal_free(&root->as.number.as.d);
            } else if (root->as.number.type == VAL_COMPLEX) {
                complex_free(&root->as.number.as.c);
            }
            break;

        case NODE_VAR:
            if (root->as.var_name) {
                free(root->as.var_name);
                root->as.var_name = NULL;
            }
            break;

        case NODE_BINARY:
            free_expr_node(root->as.bin.left);
            free_expr_node(root->as.bin.right);
            break;

        case NODE_UNARY:
            free_expr_node(root->as.un.operand);
            break;

        case NODE_CALL:
            if (root->as.call.args) {
                for (size_t i = 0; i < root->as.call.argc; i++) {
                    free_expr_node(root->as.call.args[i]);
                }
                free(root->as.call.args);
                root->as.call.args = NULL;
            }
            break;
    }

    free(root);
}
