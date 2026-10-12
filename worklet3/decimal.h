#ifndef DECIMAL_H
#define DECIMAL_H

#include "bigint.h"
#include "fast_math.h"

typedef enum {
    ROUND_HALF_UP,
    ROUND_DOWN,
    ROUND_TRUNC
} RoundingMode;

typedef struct {
    BigInteger significand;
    int        scale; /* 123.456 -> significand 123456, scale 3 */
} BigDecimal;

/* -------------------------------------------------------------------------
 * Lifecycle & String I/O
 * ------------------------------------------------------------------------- */
void  decimal_init(BigDecimal *d);
void  decimal_free(BigDecimal *d);
int   decimal_copy(BigDecimal *dest, const BigDecimal *src);
void  decimal_swap(BigDecimal *a, BigDecimal *b);
void  decimal_normalize(BigDecimal *d);

int   decimal_from_string(BigDecimal *d, const char *str);
char *decimal_to_string(const BigDecimal *d);

/* -------------------------------------------------------------------------
 * Basic Decimal Arithmetic
 * ------------------------------------------------------------------------- */
int  decimal_add(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);
int  decimal_sub(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);
int  decimal_mul(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);
int  decimal_div(BigDecimal *res, const BigDecimal *a, const BigDecimal *b, int precision);
int  decimal_round(BigDecimal *d, int target_scale, RoundingMode mode);

/* Comparisons */
int  decimal_compare(const BigDecimal *a, const BigDecimal *b);
int  decimal_is_zero(const BigDecimal *d);

/* Power of 10 scaling helper */
int  bigint_mul_pow10(BigInteger *res, const BigInteger *a, size_t exp10);

#endif /* DECIMAL_H */

