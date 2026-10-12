#define _POSIX_C_SOURCE 200809L
#include "eval.h"
#include "parser.h"
#include "bigint_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Value Lifecycle & Conversions
 * ------------------------------------------------------------------------- */

void value_init(Value *v, ValueType type) {
    if (!v) return;
    v->type = type;
    switch (type) {
        case VAL_INT:
            bigint_init(&v->as.i);
            break;
        case VAL_DEC:
            decimal_init(&v->as.d);
            break;
        case VAL_COMPLEX:
            complex_init(&v->as.c);
            break;
    }
}

void value_free(Value *v) {
    if (!v) return;
    switch (v->type) {
        case VAL_INT:
            bigint_free(&v->as.i);
            break;
        case VAL_DEC:
            decimal_free(&v->as.d);
            break;
        case VAL_COMPLEX:
            complex_free(&v->as.c);
            break;
    }
}

int value_copy(Value *dest, const Value *src) {
    if (!dest || !src) return 0;
    value_free(dest);
    dest->type = src->type;

    switch (src->type) {
        case VAL_INT:
            bigint_init(&dest->as.i);
            return bigint_copy(&dest->as.i, &src->as.i);
        case VAL_DEC:
            decimal_init(&dest->as.d);
            return decimal_copy(&dest->as.d, &src->as.d);
        case VAL_COMPLEX:
            complex_init(&dest->as.c);
            return complex_copy(&dest->as.c, &src->as.c);
    }
    return 0;
}

int value_to_decimal(BigDecimal *out, const Value *v) {
    if (!out || !v) return 0;
    decimal_free(out);
    decimal_init(out);
    if (v->type == VAL_INT) {
        if (!bigint_copy(&out->significand, &v->as.i)) return 0;
        out->scale = 0;
        return 1;
    } else if (v->type == VAL_DEC) {
        return decimal_copy(out, &v->as.d);
    }
    return 0;
}

int value_to_complex(Complex *out, const Value *v) {
    if (!out || !v) return 0;
    if (v->type == VAL_INT) {
        BigDecimal r, i_zero;
        decimal_init(&r); decimal_init(&i_zero);
        bigint_copy(&r.significand, &v->as.i);
        r.scale = 0;
        complex_set(out, &r, &i_zero);
        decimal_free(&r); decimal_free(&i_zero);
        return 1;
    } else if (v->type == VAL_DEC) {
        BigDecimal i_zero;
        decimal_init(&i_zero);
        complex_set(out, &v->as.d, &i_zero);
        decimal_free(&i_zero);
        return 1;
    } else if (v->type == VAL_COMPLEX) {
        return complex_copy(out, &v->as.c);
    }
    return 0;
}

/* Lifts operand to a higher representation in place: VAL_INT -> VAL_DEC -> VAL_COMPLEX */
static int lift_value_to(Value *v, ValueType target) {
    if (v->type == target) return 1;

    if (v->type == VAL_INT && target == VAL_DEC) {
        BigDecimal dec;
        decimal_init(&dec);
        bigint_copy(&dec.significand, &v->as.i);
        dec.scale = 0;
        bigint_free(&v->as.i);
        v->type = VAL_DEC;
        v->as.d = dec;
        return 1;
    }

    if (v->type == VAL_INT && target == VAL_COMPLEX) {
        Complex comp;
        complex_init(&comp);
        bigint_copy(&comp.real.significand, &v->as.i);
        comp.real.scale = 0;
        bigint_free(&v->as.i);
        v->type = VAL_COMPLEX;
        v->as.c = comp;
        return 1;
    }

    if (v->type == VAL_DEC && target == VAL_COMPLEX) {
        Complex comp;
        complex_init(&comp);
        decimal_copy(&comp.real, &v->as.d);
        decimal_free(&v->as.d);
        v->type = VAL_COMPLEX;
        v->as.c = comp;
        return 1;
    }

    return 0;
}

int value_promote(Value *a, Value *b) {
    if (!a || !b) return 0;
    ValueType target = a->type;
    if (b->type > target) target = b->type;

    if (!lift_value_to(a, target)) return 0;
    if (!lift_value_to(b, target)) return 0;
    return 1;
}

char *value_to_string(const Value *v) {
    if (!v) return NULL;
    switch (v->type) {
        case VAL_INT:
            return bigint_to_string(&v->as.i);
        case VAL_DEC:
            return decimal_to_string(&v->as.d);
        case VAL_COMPLEX:
            return complex_to_string(&v->as.c);
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Symbol Table Implementation
 * ------------------------------------------------------------------------- */

void symtab_init(SymbolTable *st) {
    if (!st) return;
    st->symbols = NULL;
    st->count = 0;
    st->capacity = 0;
}

void symtab_free(SymbolTable *st) {
    if (!st) return;
    for (size_t i = 0; i < st->count; i++) {
        free(st->symbols[i].name);
        value_free(&st->symbols[i].value);
    }
    free(st->symbols);
    st->symbols = NULL;
    st->count = 0;
    st->capacity = 0;
}

int symtab_set(SymbolTable *st, const char *name, const Value *v) {
    if (!st || !name || !v) return 0;

    /* Check if already present */
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->symbols[i].name, name) == 0) {
            value_free(&st->symbols[i].value);
            return value_copy(&st->symbols[i].value, v);
        }
    }

    /* Expand capacity if necessary */
    if (st->count >= st->capacity) {
        size_t new_cap = (st->capacity == 0 ? 8 : st->capacity * 2);
        Symbol *new_syms = (Symbol *)realloc(st->symbols, new_cap * sizeof(Symbol));
        if (!new_syms) return 0;
        st->symbols = new_syms;
        st->capacity = new_cap;
    }

    st->symbols[st->count].name = strdup(name);
    if (!st->symbols[st->count].name) return 0;
    value_init(&st->symbols[st->count].value, v->type);
    if (!value_copy(&st->symbols[st->count].value, v)) {
        free(st->symbols[st->count].name);
        return 0;
    }
    st->count++;
    return 1;
}

int symtab_get(const SymbolTable *st, const char *name, Value *out) {
    if (!st || !name || !out) return 0;
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->symbols[i].name, name) == 0) {
            return value_copy(out, &st->symbols[i].value);
        }
    }
    return 0;
}

int symtab_preload_constants(SymbolTable *st, int precision) {
    if (!st) return 0;
    BigDecimal pi_dec, e_dec;
    decimal_init(&pi_dec); decimal_init(&e_dec);

    math_const_pi(&pi_dec, precision + 8);
    math_const_e(&e_dec, precision + 8);

    Value v_pi, v_e, v_ans, v_i;
    v_pi.type = VAL_DEC; v_pi.as.d = pi_dec;
    v_e.type = VAL_DEC;  v_e.as.d = e_dec;

    value_init(&v_ans, VAL_INT);
    bigint_clear(&v_ans.as.i);

    value_init(&v_i, VAL_COMPLEX);
    BigDecimal i_zero, i_one;
    decimal_init(&i_zero); decimal_init(&i_one);
    decimal_from_string(&i_zero, "0");
    decimal_from_string(&i_one, "1");
    complex_set(&v_i.as.c, &i_zero, &i_one);
    decimal_free(&i_zero);
    decimal_free(&i_one);

    int ok = symtab_set(st, "pi", &v_pi) &&
             symtab_set(st, "e", &v_e) &&
             symtab_set(st, "i", &v_i) &&
             symtab_set(st, "ans", &v_ans);

    value_free(&v_pi);
    value_free(&v_e);
    value_free(&v_i);
    value_free(&v_ans);
    return ok;
}

/* -------------------------------------------------------------------------
 * EvalContext Initialization
 * ------------------------------------------------------------------------- */

void eval_context_init(EvalContext *ctx, SymbolTable *st) {
    if (!ctx) return;
    ctx->precision = 20;
    ctx->angle_mode = ANGLE_RADIAN;
    ctx->rounding = ROUND_HALF_UP;
    ctx->vars = st;
    ctx->depth = 0;
    ctx->error.code = ERR_NONE;
    ctx->error.pos = 0;
    ctx->error.message[0] = '\0';
}

static void set_error(EvalContext *ctx, EvalErrorCode code, size_t pos, const char *msg) {
    if (!ctx || ctx->error.code != ERR_NONE) return;
    ctx->error.code = code;
    ctx->error.pos = pos;
    snprintf(ctx->error.message, sizeof(ctx->error.message), "%s", msg);
}

/* -------------------------------------------------------------------------
 * Factorial Helper on BigInteger
 * ------------------------------------------------------------------------- */
static int bigint_factorial(BigInteger *res, const BigInteger *n) {
    if (n->sign < 0) return 0;
    if (bigint_is_zero(n)) {
        bigint_set_u64(res, 1);
        return 1;
    }

    /* Convert to uint64 for practical loop */
    char *s = bigint_to_string(n);
    if (!s) return 0;
    unsigned long long n_val = strtoull(s, NULL, 10);
    free(s);

    if (n_val > 10000) return 0; /* Exceeds bounds */

    bigint_set_u64(res, 1);
    for (unsigned long long i = 2; i <= n_val; i++) {
        if (!bigint_mul_small(res, res, (uint32_t)i)) return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * AST Operations Evaluator
 * ------------------------------------------------------------------------- */

int eval_binary(OpCode op, const Value *left, const Value *right, EvalContext *ctx, size_t pos, Value *out) {
    if (!left || !right || !ctx || !out) return 0;
    value_free(out);

    Value l, r;
    value_init(&l, left->type);  value_copy(&l, left);
    value_init(&r, right->type); value_copy(&r, right);

    if (!value_promote(&l, &r)) {
        value_free(&l); value_free(&r);
        set_error(ctx, ERR_TYPE_MISMATCH, pos, "Operand promotion failed");
        return 0;
    }

    int success = 0;

    if (l.type == VAL_INT) {
        value_init(out, VAL_INT);
        switch (op) {
            case OP_ADD:
                success = bigint_add(&out->as.i, &l.as.i, &r.as.i);
                break;
            case OP_SUB:
                success = bigint_sub(&out->as.i, &l.as.i, &r.as.i);
                break;
            case OP_MUL:
                success = bigint_mul(&out->as.i, &l.as.i, &r.as.i);
                break;
            case OP_DIV:
                if (bigint_is_zero(&r.as.i)) {
                    set_error(ctx, ERR_DIV_BY_ZERO, pos, "Division by zero");
                    success = 0;
                } else {
                    DivisionResult dres;
                    division_result_init(&dres);
                    if (bigint_divide(&dres, &l.as.i, &r.as.i)) {
                        /* If remainder is non-zero, promote division result to BigDecimal */
                        if (!bigint_is_zero(&dres.remainder)) {
                            value_free(out);
                            value_init(out, VAL_DEC);
                            BigDecimal num, den;
                            decimal_init(&num); decimal_init(&den);
                            bigint_copy(&num.significand, &l.as.i);
                            bigint_copy(&den.significand, &r.as.i);
                            success = decimal_div(&out->as.d, &num, &den, ctx->precision);
                            decimal_free(&num); decimal_free(&den);
                        } else {
                            bigint_copy(&out->as.i, &dres.quotient);
                            success = 1;
                        }
                    }
                    division_result_free(&dres);
                }
                break;
            case OP_MOD:
                if (bigint_is_zero(&r.as.i)) {
                    set_error(ctx, ERR_DIV_BY_ZERO, pos, "Modulo by zero");
                    success = 0;
                } else {
                    success = bigint_mod(&out->as.i, &l.as.i, &r.as.i);
                }
                break;
            case OP_POW: {
                char *exp_str = bigint_to_string(&r.as.i);
                if (!exp_str) {
                    success = 0;
                    break;
                }
                if (r.as.i.sign < 0) {
                    /* Promote to BigDecimal */
                    value_free(out);
                    value_init(out, VAL_DEC);
                    BigDecimal b_dec, res_dec, one;
                    decimal_init(&b_dec); decimal_init(&res_dec); decimal_init(&one);
                    bigint_copy(&b_dec.significand, &l.as.i);
                    decimal_from_string(&one, "1");

                    uint64_t u_exp = strtoull(exp_str + 1, NULL, 10);
                    BigInteger p_int;
                    bigint_init(&p_int);
                    bigint_pow(&p_int, &l.as.i, u_exp);
                    bigint_copy(&res_dec.significand, &p_int);
                    success = decimal_div(&out->as.d, &one, &res_dec, ctx->precision);

                    bigint_free(&p_int);
                    decimal_free(&b_dec); decimal_free(&res_dec); decimal_free(&one);
                } else {
                    uint64_t u_exp = strtoull(exp_str, NULL, 10);
                    success = bigint_pow(&out->as.i, &l.as.i, u_exp);
                }
                free(exp_str);
                break;
            }
            default:
                set_error(ctx, ERR_INTERNAL, pos, "Unsupported operator on integer");
                break;
        }
    } else if (l.type == VAL_DEC) {
        value_init(out, VAL_DEC);
        switch (op) {
            case OP_ADD:
                success = decimal_add(&out->as.d, &l.as.d, &r.as.d);
                break;
            case OP_SUB:
                success = decimal_sub(&out->as.d, &l.as.d, &r.as.d);
                break;
            case OP_MUL:
                success = decimal_mul(&out->as.d, &l.as.d, &r.as.d);
                break;
            case OP_DIV:
                if (decimal_is_zero(&r.as.d)) {
                    set_error(ctx, ERR_DIV_BY_ZERO, pos, "Division by zero");
                    success = 0;
                } else {
                    success = decimal_div(&out->as.d, &l.as.d, &r.as.d, ctx->precision);
                }
                break;
            case OP_POW: {
                /* exp(r * ln(l)) for positive base */
                if (l.as.d.significand.sign <= 0) {
                    set_error(ctx, ERR_DOMAIN, pos, "Negative base for decimal power");
                    success = 0;
                } else {
                    BigDecimal ln_l, prod;
                    decimal_init(&ln_l); decimal_init(&prod);
                    if (fn_ln(&ln_l, &l.as.d, ctx->precision + 8) &&
                        decimal_mul(&prod, &r.as.d, &ln_l)) {
                        success = fn_exp(&out->as.d, &prod, ctx->precision);
                    }
                    decimal_free(&ln_l); decimal_free(&prod);
                }
                break;
            }
            default:
                set_error(ctx, ERR_INTERNAL, pos, "Unsupported operator on decimal");
                break;
        }
    } else if (l.type == VAL_COMPLEX) {
        value_init(out, VAL_COMPLEX);
        switch (op) {
            case OP_ADD:
                success = complex_add(&out->as.c, &l.as.c, &r.as.c);
                break;
            case OP_SUB:
                success = complex_sub(&out->as.c, &l.as.c, &r.as.c);
                break;
            case OP_MUL:
                success = complex_mul(&out->as.c, &l.as.c, &r.as.c);
                break;
            case OP_DIV:
                if (complex_is_zero(&r.as.c)) {
                    set_error(ctx, ERR_DIV_BY_ZERO, pos, "Division by complex zero");
                    success = 0;
                } else {
                    success = complex_div(&out->as.c, &l.as.c, &r.as.c, ctx->precision);
                }
                break;
            default:
                set_error(ctx, ERR_INTERNAL, pos, "Unsupported operator on complex");
                break;
        }
    }

    value_free(&l);
    value_free(&r);
    return success;
}

int eval_unary(OpCode op, const Value *operand, EvalContext *ctx, size_t pos, Value *out) {
    if (!operand || !ctx || !out) return 0;
    value_free(out);

    int success = 0;
    if (op == OP_POS) {
        return value_copy(out, operand);
    } else if (op == OP_NEG) {
        if (!value_copy(out, operand)) return 0;
        if (out->type == VAL_INT) {
            out->as.i.sign = -out->as.i.sign;
        } else if (out->type == VAL_DEC) {
            out->as.d.significand.sign = -out->as.d.significand.sign;
        } else if (out->type == VAL_COMPLEX) {
            out->as.c.real.significand.sign = -out->as.c.real.significand.sign;
            out->as.c.imag.significand.sign = -out->as.c.imag.significand.sign;
        }
        return 1;
    } else if (op == OP_FACT) {
        if (operand->type != VAL_INT) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "Factorial requires non-negative integer");
            return 0;
        }
        value_init(out, VAL_INT);
        success = bigint_factorial(&out->as.i, &operand->as.i);
        if (!success) {
            set_error(ctx, ERR_DOMAIN, pos, "Factorial input negative or too large");
        }
        return success;
    }

    set_error(ctx, ERR_INTERNAL, pos, "Unknown unary operator");
    return 0;
}

int eval_call(int fn_id, const Value *args, size_t argc, EvalContext *ctx, size_t pos, Value *out) {
    (void)argc;
    if (!args || !ctx || !out) return 0;
    value_free(out);
    if (fn_id < 0 || (size_t)fn_id >= FUNCTION_COUNT) {
        set_error(ctx, ERR_UNDEFINED_FUNC, pos, "Invalid function ID");
        return 0;
    }

    const char *name = FUNCTIONS[fn_id].name;
    int success = 0;

    if (strcmp(name, "sin") == 0) {
        BigDecimal dec, rad;
        decimal_init(&dec); decimal_init(&rad);
        if (!value_to_decimal(&dec, &args[0])) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "sin requires numeric argument");
        } else {
            if (ctx->angle_mode == ANGLE_DEGREE) {
                BigDecimal pi, one_eighty;
                decimal_init(&pi); decimal_init(&one_eighty);
                math_const_pi(&pi, ctx->precision + 8);
                decimal_from_string(&one_eighty, "180");
                decimal_mul(&rad, &dec, &pi);
                decimal_div(&rad, &rad, &one_eighty, ctx->precision + 8);
                decimal_free(&pi); decimal_free(&one_eighty);
            } else {
                decimal_copy(&rad, &dec);
            }
            value_init(out, VAL_DEC);
            success = fn_sin(&out->as.d, &rad, ctx->precision);
        }
        decimal_free(&dec); decimal_free(&rad);
    } else if (strcmp(name, "cos") == 0) {
        BigDecimal dec, rad;
        decimal_init(&dec); decimal_init(&rad);
        if (!value_to_decimal(&dec, &args[0])) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "cos requires numeric argument");
        } else {
            if (ctx->angle_mode == ANGLE_DEGREE) {
                BigDecimal pi, one_eighty;
                decimal_init(&pi); decimal_init(&one_eighty);
                math_const_pi(&pi, ctx->precision + 8);
                decimal_from_string(&one_eighty, "180");
                decimal_mul(&rad, &dec, &pi);
                decimal_div(&rad, &rad, &one_eighty, ctx->precision + 8);
                decimal_free(&pi); decimal_free(&one_eighty);
            } else {
                decimal_copy(&rad, &dec);
            }
            value_init(out, VAL_DEC);
            success = fn_cos(&out->as.d, &rad, ctx->precision);
        }
        decimal_free(&dec); decimal_free(&rad);
    } else if (strcmp(name, "exp") == 0) {
        BigDecimal dec;
        decimal_init(&dec);
        if (!value_to_decimal(&dec, &args[0])) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "exp requires numeric argument");
        } else {
            value_init(out, VAL_DEC);
            success = fn_exp(&out->as.d, &dec, ctx->precision);
        }
        decimal_free(&dec);
    } else if (strcmp(name, "ln") == 0) {
        BigDecimal dec;
        decimal_init(&dec);
        if (!value_to_decimal(&dec, &args[0])) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "ln requires numeric argument");
        } else {
            value_init(out, VAL_DEC);
            success = fn_ln(&out->as.d, &dec, ctx->precision);
            if (!success) {
                set_error(ctx, ERR_DOMAIN, pos, "Domain error: ln argument <= 0");
            }
        }
        decimal_free(&dec);
    } else if (strcmp(name, "sqrt") == 0) {
        BigDecimal dec;
        decimal_init(&dec);
        if (!value_to_decimal(&dec, &args[0])) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "sqrt requires numeric argument");
        } else {
            value_init(out, VAL_DEC);
            success = fn_sqrt(&out->as.d, &dec, ctx->precision);
            if (!success) {
                set_error(ctx, ERR_DOMAIN, pos, "Domain error: negative sqrt argument");
            }
        }
        decimal_free(&dec);
    } else if (strcmp(name, "abs") == 0) {
        if (args[0].type == VAL_INT) {
            value_init(out, VAL_INT);
            bigint_copy(&out->as.i, &args[0].as.i);
            out->as.i.sign = (bigint_is_zero(&out->as.i) ? 0 : 1);
            success = 1;
        } else if (args[0].type == VAL_DEC) {
            value_init(out, VAL_DEC);
            decimal_copy(&out->as.d, &args[0].as.d);
            out->as.d.significand.sign = (decimal_is_zero(&out->as.d) ? 0 : 1);
            success = 1;
        } else if (args[0].type == VAL_COMPLEX) {
            /* |a + bi| = sqrt(a^2 + b^2) */
            BigDecimal r2, i2, sum_sq;
            decimal_init(&r2); decimal_init(&i2); decimal_init(&sum_sq);
            decimal_mul(&r2, &args[0].as.c.real, &args[0].as.c.real);
            decimal_mul(&i2, &args[0].as.c.imag, &args[0].as.c.imag);
            decimal_add(&sum_sq, &r2, &i2);
            value_init(out, VAL_DEC);
            success = fn_sqrt(&out->as.d, &sum_sq, ctx->precision);
            decimal_free(&r2); decimal_free(&i2); decimal_free(&sum_sq);
        }
    } else if (strcmp(name, "gcd") == 0) {
        if (args[0].type != VAL_INT || args[1].type != VAL_INT) {
            set_error(ctx, ERR_TYPE_MISMATCH, pos, "gcd requires integer arguments");
        } else {
            value_init(out, VAL_INT);
            success = bigint_gcd(&out->as.i, &args[0].as.i, &args[1].as.i);
        }
    } else if (strcmp(name, "pow") == 0) {
        return eval_binary(OP_POW, &args[0], &args[1], ctx, pos, out);
    } else if (strcmp(name, "mod") == 0) {
        return eval_binary(OP_MOD, &args[0], &args[1], ctx, pos, out);
    } else if (strcmp(name, "fact") == 0) {
        return eval_unary(OP_FACT, &args[0], ctx, pos, out);
    } else {
        set_error(ctx, ERR_UNDEFINED_FUNC, pos, "Unrecognized function");
    }

    return success;
}

/* -------------------------------------------------------------------------
 * Recursive Post-Order Tree Walker
 * ------------------------------------------------------------------------- */

int eval_node(const ExprNode *n, EvalContext *ctx, Value *out) {
    if (!n || !ctx || !out) return 0;

    if (ctx->depth > MAX_RECURSION_DEPTH) {
        set_error(ctx, ERR_RECURSION_LIMIT, n->pos, "Recursion depth limit exceeded");
        return 0;
    }

    ctx->depth++;
    int success = 0;
    value_free(out);

    switch (n->type) {
        case NODE_NUMBER:
            success = value_copy(out, &n->as.number);
            break;

        case NODE_VAR:
            if (!symtab_get(ctx->vars, n->as.var_name, out)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Undefined variable '%s'", n->as.var_name);
                set_error(ctx, ERR_UNDEFINED_VAR, n->pos, msg);
                success = 0;
            } else {
                success = 1;
            }
            break;

        case NODE_BINARY: {
            Value l, r;
            value_init(&l, VAL_INT);
            value_init(&r, VAL_INT);

            if (eval_node(n->as.bin.left, ctx, &l) && eval_node(n->as.bin.right, ctx, &r)) {
                success = eval_binary(n->as.bin.op, &l, &r, ctx, n->pos, out);
            }
            value_free(&l);
            value_free(&r);
            break;
        }

        case NODE_UNARY: {
            Value operand;
            value_init(&operand, VAL_INT);
            if (eval_node(n->as.un.operand, ctx, &operand)) {
                success = eval_unary(n->as.un.op, &operand, ctx, n->pos, out);
            }
            value_free(&operand);
            break;
        }

        case NODE_CALL: {
            Value *eval_args = (Value *)calloc(n->as.call.argc, sizeof(Value));
            if (!eval_args && n->as.call.argc > 0) {
                set_error(ctx, ERR_INTERNAL, n->pos, "Memory allocation failure");
                ctx->depth--;
                return 0;
            }

            int all_args_ok = 1;
            for (size_t i = 0; i < n->as.call.argc; i++) {
                value_init(&eval_args[i], VAL_INT);
                if (!eval_node(n->as.call.args[i], ctx, &eval_args[i])) {
                    all_args_ok = 0;
                    break;
                }
            }

            if (all_args_ok) {
                success = eval_call(n->as.call.fn_id, eval_args, n->as.call.argc, ctx, n->pos, out);
            }

            for (size_t i = 0; i < n->as.call.argc; i++) {
                value_free(&eval_args[i]);
            }
            free(eval_args);
            break;
        }
    }

    ctx->depth--;
    return success;
}
