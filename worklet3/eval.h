#ifndef EVAL_H
#define EVAL_H

#include <stddef.h>
#include "ast.h"
#include "functions.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RECURSION_DEPTH 128

typedef enum {
    ANGLE_RADIAN,
    ANGLE_DEGREE
} AngleMode;

typedef enum {
    ERR_NONE = 0,
    ERR_DIV_BY_ZERO,
    ERR_UNDEFINED_VAR,
    ERR_UNDEFINED_FUNC,
    ERR_INVALID_ARITY,
    ERR_TYPE_MISMATCH,
    ERR_DOMAIN,
    ERR_RECURSION_LIMIT,
    ERR_OVERFLOW,
    ERR_INTERNAL
} EvalErrorCode;

typedef struct {
    EvalErrorCode code;
    size_t        pos;
    char          message[256];
} EvalError;

/* -------------------------------------------------------------------------
 * Symbol Table (Variables & Constants)
 * ------------------------------------------------------------------------- */
typedef struct {
    char  *name;
    Value  value;
} Symbol;

typedef struct {
    Symbol *symbols;
    size_t  count;
    size_t  capacity;
} SymbolTable;

void symtab_init(SymbolTable *st);
void symtab_free(SymbolTable *st);
int  symtab_set(SymbolTable *st, const char *name, const Value *v);
int  symtab_get(const SymbolTable *st, const char *name, Value *out);
int  symtab_preload_constants(SymbolTable *st, int precision);

/* -------------------------------------------------------------------------
 * Evaluation Context
 * ------------------------------------------------------------------------- */
typedef struct {
    int          precision;
    AngleMode    angle_mode; /* RADIAN or DEGREE */
    RoundingMode rounding;
    SymbolTable *vars;
    int          depth;      /* Recursion depth guard */
    EvalError    error;
} EvalContext;

void eval_context_init(EvalContext *ctx, SymbolTable *st);

/* -------------------------------------------------------------------------
 * Value System Functions
 * ------------------------------------------------------------------------- */
void  value_init(Value *v, ValueType type);
void  value_free(Value *v);
int   value_copy(Value *dest, const Value *src);
int   value_promote(Value *a, Value *b);
char *value_to_string(const Value *v);

/* Value conversion helpers */
int   value_to_decimal(BigDecimal *out, const Value *v);
int   value_to_complex(Complex *out, const Value *v);

/* -------------------------------------------------------------------------
 * AST Evaluator
 * ------------------------------------------------------------------------- */
int eval_node(const ExprNode *n, EvalContext *ctx, Value *out);
int eval_binary(OpCode op, const Value *left, const Value *right, EvalContext *ctx, size_t pos, Value *out);
int eval_unary(OpCode op, const Value *operand, EvalContext *ctx, size_t pos, Value *out);
int eval_call(int fn_id, const Value *args, size_t argc, EvalContext *ctx, size_t pos, Value *out);

#ifdef __cplusplus
}
#endif

#endif /* EVAL_H */

