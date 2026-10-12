#ifndef BIGINT_H
#define BIGINT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BASE 1000000000U
#define BASE_DIGITS 9

typedef struct {
    int        sign;     /* +1, -1, or 0 */
    size_t     size;     /* Active limbs in use */
    size_t     capacity; /* Allocated limbs */
    uint32_t  *digits;   /* Dynamic base-10^9 limb array (least significant first) */
} BigInteger;

/* -------------------------------------------------------------------------
 * Lifecycle, Normalization & Memory Management
 * ------------------------------------------------------------------------- */
void bigint_init(BigInteger *a);
int  bigint_init_capacity(BigInteger *a, size_t cap);
int  bigint_reserve(BigInteger *a, size_t min_cap);
int  bigint_resize(BigInteger *a, size_t new_size);
void bigint_clear(BigInteger *a);
void bigint_free(BigInteger *a);
int  bigint_copy(BigInteger *dest, const BigInteger *src);
void bigint_swap(BigInteger *a, BigInteger *b);
void bigint_normalize(BigInteger *a);

/* -------------------------------------------------------------------------
 * Value Inspection & Predicates
 * ------------------------------------------------------------------------- */
int  bigint_is_zero(const BigInteger *a);
int  bigint_is_one(const BigInteger *a);

/* -------------------------------------------------------------------------
 * Comparisons
 * ------------------------------------------------------------------------- */
int  bigint_compare_abs(const BigInteger *a, const BigInteger *b);
int  bigint_compare(const BigInteger *a, const BigInteger *b);

int  bigint_equal(const BigInteger *a, const BigInteger *b);
int  bigint_less(const BigInteger *a, const BigInteger *b);
int  bigint_greater(const BigInteger *a, const BigInteger *b);
int  bigint_less_equal(const BigInteger *a, const BigInteger *b);
int  bigint_greater_equal(const BigInteger *a, const BigInteger *b);

/* -------------------------------------------------------------------------
 * Addition & Subtraction
 * ------------------------------------------------------------------------- */
int  bigint_add_abs(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_sub_abs(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_add(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_sub(BigInteger *res, const BigInteger *a, const BigInteger *b);
int  bigint_add_small(BigInteger *res, const BigInteger *a, int64_t val);

/* -------------------------------------------------------------------------
 * Multiplication
 * ------------------------------------------------------------------------- */
int  bigint_mul_small(BigInteger *res, const BigInteger *a, uint32_t val);
int  bigint_mul_schoolbook(BigInteger *res, const BigInteger *a, const BigInteger *b);

/* -------------------------------------------------------------------------
 * Helper Conversions for Testing
 * ------------------------------------------------------------------------- */
int  bigint_set_u64(BigInteger *a, uint64_t val);
int  bigint_set_i64(BigInteger *a, int64_t val);

#endif /* BIGINT_H */

