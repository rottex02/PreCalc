#include "fast_math.h"
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Division Result Lifecycle
 * ------------------------------------------------------------------------- */

void division_result_init(DivisionResult *res) {
    if (!res) return;
    bigint_init(&res->quotient);
    bigint_init(&res->remainder);
}

void division_result_free(DivisionResult *res) {
    if (!res) return;
    bigint_free(&res->quotient);
    bigint_free(&res->remainder);
}

/* -------------------------------------------------------------------------
 * Shift & Slice Helpers
 * ------------------------------------------------------------------------- */

int bigint_shift_limbs(BigInteger *res, const BigInteger *a, size_t shift) {
    if (!res || !a) return 0;

    if (bigint_is_zero(a) || shift == 0) {
        return bigint_copy(res, a);
    }

    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, a->size + shift)) return 0;

    memset(tmp.digits, 0, shift * sizeof(uint32_t));
    memcpy(tmp.digits + shift, a->digits, a->size * sizeof(uint32_t));
    tmp.size = a->size + shift;
    tmp.sign = a->sign;
    bigint_normalize(&tmp);

    bigint_swap(res, &tmp);
    bigint_free(&tmp);
    return 1;
}

int bigint_slice(BigInteger *dest, const BigInteger *src, size_t start, size_t len) {
    if (!dest || !src) return 0;

    if (start >= src->size || len == 0) {
        bigint_clear(dest);
        return 1;
    }

    if (start + len > src->size) {
        len = src->size - start;
    }

    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, len)) return 0;

    memcpy(tmp.digits, src->digits + start, len * sizeof(uint32_t));
    tmp.size = len;
    tmp.sign = 1;
    bigint_normalize(&tmp);

    bigint_swap(dest, &tmp);
    bigint_free(&tmp);
    return 1;
}

/* -------------------------------------------------------------------------
 * Karatsuba Fast Multiplication
 * ------------------------------------------------------------------------- */

int bigint_mul_karatsuba(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    if (bigint_is_zero(a) || bigint_is_zero(b)) {
        bigint_clear(res);
        return 1;
    }

    /* Base case: delegate to schoolbook multiplication below crossover */
    if (a->size < KARATSUBA_CROSSOVER || b->size < KARATSUBA_CROSSOVER) {
        return bigint_mul_schoolbook(res, a, b);
    }

    size_t n = (a->size > b->size ? a->size : b->size);
    size_t k = (n + 1) / 2;

    BigInteger a0, a1, b0, b1;
    bigint_init(&a0); bigint_init(&a1);
    bigint_init(&b0); bigint_init(&b1);

    if (!bigint_slice(&a0, a, 0, k) ||
        !bigint_slice(&a1, a, k, a->size > k ? a->size - k : 0) ||
        !bigint_slice(&b0, b, 0, k) ||
        !bigint_slice(&b1, b, k, b->size > k ? b->size - k : 0)) {
        bigint_free(&a0); bigint_free(&a1);
        bigint_free(&b0); bigint_free(&b1);
        return 0;
    }

    /* z2 = a1 * b1 */
    BigInteger z2;
    bigint_init(&z2);
    if (!bigint_mul_karatsuba(&z2, &a1, &b1)) goto cleanup_fail;

    /* z0 = a0 * b0 */
    BigInteger z0;
    bigint_init(&z0);
    if (!bigint_mul_karatsuba(&z0, &a0, &b0)) {
        bigint_free(&z2);
        goto cleanup_fail;
    }

    /* sa = a1 + a0, sb = b1 + b0 */
    BigInteger sa, sb;
    bigint_init(&sa); bigint_init(&sb);
    if (!bigint_add(&sa, &a1, &a0) || !bigint_add(&sb, &b1, &b0)) {
        bigint_free(&sa); bigint_free(&sb);
        bigint_free(&z2); bigint_free(&z0);
        goto cleanup_fail;
    }

    /* p = sa * sb */
    BigInteger p;
    bigint_init(&p);
    if (!bigint_mul_karatsuba(&p, &sa, &sb)) {
        bigint_free(&sa); bigint_free(&sb);
        bigint_free(&p); bigint_free(&z2); bigint_free(&z0);
        goto cleanup_fail;
    }
    bigint_free(&sa);
    bigint_free(&sb);

    /* z1 = p - z2 - z0 */
    BigInteger z1;
    bigint_init(&z1);
    if (!bigint_sub(&z1, &p, &z2) || !bigint_sub(&z1, &z1, &z0)) {
        bigint_free(&p); bigint_free(&z1);
        bigint_free(&z2); bigint_free(&z0);
        goto cleanup_fail;
    }
    bigint_free(&p);

    /* Combine: z2 * B^(2k) + z1 * B^k + z0 */
    BigInteger z2_shifted, z1_shifted;
    bigint_init(&z2_shifted); bigint_init(&z1_shifted);
    if (!bigint_shift_limbs(&z2_shifted, &z2, 2 * k) ||
        !bigint_shift_limbs(&z1_shifted, &z1, k)) {
        bigint_free(&z2_shifted); bigint_free(&z1_shifted);
        bigint_free(&z1); bigint_free(&z2); bigint_free(&z0);
        goto cleanup_fail;
    }
    bigint_free(&z2);
    bigint_free(&z1);

    BigInteger total;
    bigint_init(&total);
    if (!bigint_add(&total, &z2_shifted, &z1_shifted) ||
        !bigint_add(&total, &total, &z0)) {
        bigint_free(&total);
        bigint_free(&z2_shifted); bigint_free(&z1_shifted);
        bigint_free(&z0);
        goto cleanup_fail;
    }
    bigint_free(&z2_shifted);
    bigint_free(&z1_shifted);
    bigint_free(&z0);

    total.sign = a->sign * b->sign;
    bigint_normalize(&total);

    bigint_swap(res, &total);
    bigint_free(&total);

    bigint_free(&a0); bigint_free(&a1);
    bigint_free(&b0); bigint_free(&b1);
    return 1;

cleanup_fail:
    bigint_free(&a0); bigint_free(&a1);
    bigint_free(&b0); bigint_free(&b1);
    return 0;
}

int bigint_mul(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;
    if (a->size < KARATSUBA_CROSSOVER || b->size < KARATSUBA_CROSSOVER) {
        return bigint_mul_schoolbook(res, a, b);
    }
    return bigint_mul_karatsuba(res, a, b);
}

/* -------------------------------------------------------------------------
 * Multi-Limb Long Division (Knuth Algorithm D)
 * ------------------------------------------------------------------------- */

int bigint_divide(DivisionResult *res, const BigInteger *dividend, const BigInteger *divisor) {
    if (!res || !dividend || !divisor) return 0;

    /* Division by zero */
    if (bigint_is_zero(divisor)) {
        return 0;
    }

    /* Reset existing quotient and remainder */
    bigint_clear(&res->quotient);
    bigint_clear(&res->remainder);

    /* If dividend is zero */
    if (bigint_is_zero(dividend)) {
        return 1;
    }

    int cmp = bigint_compare_abs(dividend, divisor);
    if (cmp < 0) {
        bigint_clear(&res->quotient);
        if (!bigint_copy(&res->remainder, dividend)) return 0;
        return 1;
    }
    if (cmp == 0) {
        bigint_set_u64(&res->quotient, 1);
        res->quotient.sign = dividend->sign * divisor->sign;
        bigint_clear(&res->remainder);
        return 1;
    }

    int q_sign = dividend->sign * divisor->sign;
    int r_sign = dividend->sign;

    /* Single-limb divisor optimization */
    if (divisor->size == 1) {
        uint32_t d = divisor->digits[0];
        BigInteger q;
        bigint_init_capacity(&q, dividend->size);
        q.size = dividend->size;

        uint64_t rem = 0;
        for (size_t i = dividend->size; i > 0; i--) {
            size_t idx = i - 1;
            uint64_t cur = rem * BASE + dividend->digits[idx];
            q.digits[idx] = (uint32_t)(cur / d);
            rem = cur % d;
        }

        q.sign = q_sign;
        bigint_normalize(&q);

        BigInteger r;
        bigint_init(&r);
        bigint_set_u64(&r, rem);
        r.sign = (rem == 0 ? 0 : r_sign);

        bigint_free(&res->quotient);
        res->quotient = q;
        bigint_free(&res->remainder);
        res->remainder = r;
        return 1;
    }

    /* Knuth Algorithm D for divisor->size >= 2 */
    size_t n = divisor->size;
    size_t m = dividend->size - n;

    /* Normalization factor d = BASE / (divisor->digits[n-1] + 1) */
    uint32_t d_norm = (uint32_t)(BASE / ((uint64_t)divisor->digits[n - 1] + 1));

    BigInteger u, v;
    bigint_init(&u); bigint_init(&v);

    if (!bigint_mul_small(&u, dividend, d_norm) ||
        !bigint_mul_small(&v, divisor, d_norm)) {
        bigint_free(&u); bigint_free(&v);
        return 0;
    }

    /* Ensure u has at least m + n + 1 limbs */
    if (u.size < m + n + 1) {
        if (!bigint_resize(&u, m + n + 1)) {
            bigint_free(&u); bigint_free(&v);
            return 0;
        }
    }

    BigInteger q;
    bigint_init_capacity(&q, m + 1);
    q.size = m + 1;

    uint32_t vn1 = v.digits[n - 1];
    uint32_t vn2 = v.digits[n - 2];

    for (size_t j_idx = m + 1; j_idx > 0; j_idx--) {
        size_t j = j_idx - 1;

        uint64_t num = (uint64_t)u.digits[j + n] * BASE + u.digits[j + n - 1];
        uint64_t q_hat = num / vn1;
        uint64_t r_hat = num % vn1;

        while (q_hat >= BASE || (q_hat * vn2 > r_hat * BASE + u.digits[j + n - 2])) {
            q_hat--;
            r_hat += vn1;
            if (r_hat >= BASE) break;
        }

        /* Multiply v by q_hat and subtract from u[j .. j+n] */
        uint64_t carry = 0;
        int64_t borrow = 0;

        for (size_t i = 0; i < n; i++) {
            uint64_t prod = (uint64_t)q_hat * v.digits[i] + carry;
            carry = prod / BASE;
            uint32_t p_limb = (uint32_t)(prod % BASE);

            int64_t diff = (int64_t)u.digits[j + i] - p_limb - borrow;
            if (diff < 0) {
                diff += BASE;
                borrow = 1;
            } else {
                borrow = 0;
            }
            u.digits[j + i] = (uint32_t)diff;
        }

        int64_t diff = (int64_t)u.digits[j + n] - (int64_t)carry - borrow;
        if (diff < 0) {
            diff += BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        u.digits[j + n] = (uint32_t)diff;

        /* If borrow > 0, q_hat was 1 too large: add v back to u[j .. j+n] */
        if (borrow != 0) {
            q_hat--;
            uint64_t add_carry = 0;
            for (size_t i = 0; i < n; i++) {
                uint64_t sum = (uint64_t)u.digits[j + i] + v.digits[i] + add_carry;
                u.digits[j + i] = (uint32_t)(sum % BASE);
                add_carry = sum / BASE;
            }
            u.digits[j + n] = (uint32_t)((u.digits[j + n] + add_carry) % BASE);
        }

        q.digits[j] = (uint32_t)q_hat;
    }

    q.sign = q_sign;
    bigint_normalize(&q);

    /* Unnormalize remainder: divide u[0 .. n-1] by d_norm */
    u.size = n;
    bigint_normalize(&u);

    BigInteger r;
    bigint_init(&r);

    if (d_norm > 1) {
        uint64_t rem = 0;
        for (size_t i = u.size; i > 0; i--) {
            size_t idx = i - 1;
            uint64_t cur = rem * BASE + u.digits[idx];
            u.digits[idx] = (uint32_t)(cur / d_norm);
            rem = cur % d_norm;
        }
        bigint_normalize(&u);
        bigint_swap(&r, &u);
    } else {
        bigint_swap(&r, &u);
    }

    r.sign = (bigint_is_zero(&r) ? 0 : r_sign);
    bigint_normalize(&r);

    bigint_free(&res->quotient);
    res->quotient = q;
    bigint_free(&res->remainder);
    res->remainder = r;
    bigint_free(&u);
    bigint_free(&v);
    return 1;
}

int bigint_mod(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;
    DivisionResult div_res;
    division_result_init(&div_res);

    if (!bigint_divide(&div_res, a, b)) {
        division_result_free(&div_res);
        return 0;
    }

    bigint_swap(res, &div_res.remainder);
    division_result_free(&div_res);
    return 1;
}

/* -------------------------------------------------------------------------
 * Number Theory Primitives
 * ------------------------------------------------------------------------- */

int bigint_gcd(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    BigInteger x, y, r;
    bigint_init(&x);
    bigint_init(&y);
    bigint_init(&r);

    if (!bigint_copy(&x, a) || !bigint_copy(&y, b)) {
        bigint_free(&x); bigint_free(&y); bigint_free(&r);
        return 0;
    }

    x.sign = 1;
    y.sign = 1;

    while (!bigint_is_zero(&y)) {
        if (!bigint_mod(&r, &x, &y)) {
            bigint_free(&x); bigint_free(&y); bigint_free(&r);
            return 0;
        }
        bigint_copy(&x, &y);
        bigint_copy(&y, &r);
    }

    x.sign = (bigint_is_zero(&x) ? 0 : 1);
    bigint_swap(res, &x);

    bigint_free(&x);
    bigint_free(&y);
    bigint_free(&r);
    return 1;
}

int bigint_pow(BigInteger *res, const BigInteger *base, uint64_t exp) {
    if (!res || !base) return 0;

    BigInteger result, cur;
    bigint_init(&result);
    bigint_init(&cur);

    if (!bigint_set_u64(&result, 1) || !bigint_copy(&cur, base)) {
        bigint_free(&result);
        bigint_free(&cur);
        return 0;
    }

    while (exp > 0) {
        if (exp & 1) {
            if (!bigint_mul(&result, &result, &cur)) {
                bigint_free(&result);
                bigint_free(&cur);
                return 0;
            }
        }
        if (exp > 1) {
            if (!bigint_mul(&cur, &cur, &cur)) {
                bigint_free(&result);
                bigint_free(&cur);
                return 0;
            }
        }
        exp >>= 1;
    }

    bigint_swap(res, &result);
    bigint_free(&result);
    bigint_free(&cur);
    return 1;
}

int bigint_mod_pow(BigInteger *res, const BigInteger *base, const BigInteger *exp, const BigInteger *mod) {
    if (!res || !base || !exp || !mod || bigint_is_zero(mod)) return 0;

    BigInteger result, cur, exp_copy, two, r;
    bigint_init(&result);
    bigint_init(&cur);
    bigint_init(&exp_copy);
    bigint_init(&two);
    bigint_init(&r);

    bigint_set_u64(&result, 1);
    bigint_set_u64(&two, 2);
    bigint_mod(&cur, base, mod);
    bigint_copy(&exp_copy, exp);

    while (!bigint_is_zero(&exp_copy)) {
        /* Check if exp_copy is odd */
        DivisionResult div_res;
        division_result_init(&div_res);
        bigint_divide(&div_res, &exp_copy, &two);

        if (!bigint_is_zero(&div_res.remainder)) {
            bigint_mul(&result, &result, &cur);
            bigint_mod(&result, &result, mod);
        }

        bigint_copy(&exp_copy, &div_res.quotient);
        division_result_free(&div_res);

        if (!bigint_is_zero(&exp_copy)) {
            bigint_mul(&cur, &cur, &cur);
            bigint_mod(&cur, &cur, mod);
        }
    }

    bigint_swap(res, &result);

    bigint_free(&result);
    bigint_free(&cur);
    bigint_free(&exp_copy);
    bigint_free(&two);
    bigint_free(&r);
    return 1;
}
