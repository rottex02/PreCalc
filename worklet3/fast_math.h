#ifndef FAST_MATH_H
#define FAST_MATH_H

#include "bigint.h"

/* Crossover threshold between Schoolbook and Karatsuba */
#define KARATSUBA_CROSSOVER 16

/* -------------------------------------------------------------------------
 * Division Result Struct
 * ------------------------------------------------------------------------- */
typedef struct {
    BigInteger quotient;
    BigInteger remainder;
} DivisionResult;

void division_result_init(DivisionResult *res);
void division_result_free(DivisionResult *res);

/* -------------------------------------------------------------------------
 * Fast Multiplication & Shift Utilities
 * ------------------------------------------------------------------------- */
int  bigint_shift_limbs(BigInteger *res, const BigInteger *a, size_t shift);
int  bigint_slice(BigInteger *dest, const BigInteger *src, size_t start, size_t len);

int  bigint_mul_karatsuba(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_mul(BigInteger *res, const BigInteger *a, const BigInteger *b);

/* -------------------------------------------------------------------------
 * Multi-Limb Long Division & Modulo
 * ------------------------------------------------------------------------- */
int  bigint_divide(DivisionResult *res, const BigInteger *dividend, const BigInteger *divisor);
int  bigint_mod(BigInteger *res, const BigInteger *a, const BigInteger *b);

/* -------------------------------------------------------------------------
 * Number Theory Primitives
 * ------------------------------------------------------------------------- */
int  bigint_gcd(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_pow(BigInteger *res, const BigInteger *base, uint64_t exp);
int  bigint_mod_pow(BigInteger *res, const BigInteger *base, const BigInteger *exp, const BigInteger *mod);

#endif /* FAST_MATH_H */

