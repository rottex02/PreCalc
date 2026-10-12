#include "bigint.h"
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Lifecycle, Normalization & Memory Management
 * ------------------------------------------------------------------------- */

void bigint_init(BigInteger *a) {
    if (!a) return;
    a->sign = 0;
    a->size = 1;
    a->capacity = 1;
    a->digits = (uint32_t *)calloc(1, sizeof(uint32_t));
}

int bigint_init_capacity(BigInteger *a, size_t cap) {
    if (!a) return 0;
    if (cap == 0) cap = 1;
    a->sign = 0;
    a->size = 1;
    a->capacity = cap;
    a->digits = (uint32_t *)calloc(cap, sizeof(uint32_t));
    return (a->digits != NULL);
}

int bigint_reserve(BigInteger *a, size_t min_cap) {
    if (!a) return 0;
    if (a->capacity >= min_cap) return 1;

    size_t new_cap = a->capacity == 0 ? 1 : a->capacity;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    uint32_t *new_digits = (uint32_t *)realloc(a->digits, new_cap * sizeof(uint32_t));
    if (!new_digits) return 0;

    /* Zero out newly allocated limbs */
    memset(new_digits + a->capacity, 0, (new_cap - a->capacity) * sizeof(uint32_t));

    a->digits = new_digits;
    a->capacity = new_cap;
    return 1;
}

int bigint_resize(BigInteger *a, size_t new_size) {
    if (!a) return 0;
    if (new_size == 0) new_size = 1;
    if (!bigint_reserve(a, new_size)) return 0;
    if (new_size > a->size) {
        memset(a->digits + a->size, 0, (new_size - a->size) * sizeof(uint32_t));
    }
    a->size = new_size;
    return 1;
}

void bigint_clear(BigInteger *a) {
    if (!a) return;
    if (a->digits && a->capacity > 0) {
        memset(a->digits, 0, a->capacity * sizeof(uint32_t));
    }
    a->sign = 0;
    a->size = 1;
}

void bigint_free(BigInteger *a) {
    if (!a) return;
    if (a->digits) {
        free(a->digits);
        a->digits = NULL;
    }
    a->capacity = 0;
    a->size = 0;
    a->sign = 0;
}

int bigint_copy(BigInteger *dest, const BigInteger *src) {
    if (!dest || !src) return 0;
    if (dest == src) return 1;

    if (!bigint_reserve(dest, src->size)) return 0;
    memcpy(dest->digits, src->digits, src->size * sizeof(uint32_t));
    dest->size = src->size;
    dest->sign = src->sign;
    return 1;
}

void bigint_swap(BigInteger *a, BigInteger *b) {
    if (!a || !b || a == b) return;
    BigInteger tmp = *a;
    *a = *b;
    *b = tmp;
}

void bigint_normalize(BigInteger *a) {
    if (!a || !a->digits || a->size == 0) return;

    /* Strip leading zero limbs */
    while (a->size > 1 && a->digits[a->size - 1] == 0) {
        a->size--;
    }

    /* Enforce canonical zero invariant: size = 1, digits[0] = 0, sign = 0 */
    if (a->size == 1 && a->digits[0] == 0) {
        a->sign = 0;
    } else if (a->sign == 0) {
        /* Default non-zero numbers with undefined sign to +1 */
        a->sign = 1;
    }
}

/* -------------------------------------------------------------------------
 * Value Inspection & Predicates
 * ------------------------------------------------------------------------- */

int bigint_is_zero(const BigInteger *a) {
    if (!a || !a->digits || a->size == 0) return 1;
    return (a->size == 1 && a->digits[0] == 0);
}

int bigint_is_one(const BigInteger *a) {
    if (!a || !a->digits) return 0;
    return (a->sign == 1 && a->size == 1 && a->digits[0] == 1);
}

/* -------------------------------------------------------------------------
 * Comparisons
 * ------------------------------------------------------------------------- */

int bigint_compare_abs(const BigInteger *a, const BigInteger *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    /* Normalize checks on sizes */
    size_t a_size = a->size;
    size_t b_size = b->size;

    while (a_size > 1 && a->digits[a_size - 1] == 0) a_size--;
    while (b_size > 1 && b->digits[b_size - 1] == 0) b_size--;

    if (a_size < b_size) return -1;
    if (a_size > b_size) return 1;

    /* Limb-by-limb check from MSB down to LSB */
    for (size_t i = a_size; i > 0; i--) {
        size_t idx = i - 1;
        if (a->digits[idx] < b->digits[idx]) return -1;
        if (a->digits[idx] > b->digits[idx]) return 1;
    }
    return 0;
}

int bigint_compare(const BigInteger *a, const BigInteger *b) {
    if (!a && !b) return 0;
    if (!a) return (b->sign >= 0 ? -1 : 1);
    if (!b) return (a->sign >= 0 ? 1 : -1);

    int a_zero = bigint_is_zero(a);
    int b_zero = bigint_is_zero(b);

    if (a_zero && b_zero) return 0;
    if (a_zero) return (b->sign > 0 ? -1 : 1);
    if (b_zero) return (a->sign > 0 ? 1 : -1);

    if (a->sign != b->sign) {
        return (a->sign > b->sign ? 1 : -1);
    }

    int abs_cmp = bigint_compare_abs(a, b);
    return (a->sign > 0 ? abs_cmp : -abs_cmp);
}

int bigint_equal(const BigInteger *a, const BigInteger *b) {
    return bigint_compare(a, b) == 0;
}

int bigint_less(const BigInteger *a, const BigInteger *b) {
    return bigint_compare(a, b) < 0;
}

int bigint_greater(const BigInteger *a, const BigInteger *b) {
    return bigint_compare(a, b) > 0;
}

int bigint_less_equal(const BigInteger *a, const BigInteger *b) {
    return bigint_compare(a, b) <= 0;
}

int bigint_greater_equal(const BigInteger *a, const BigInteger *b) {
    return bigint_compare(a, b) >= 0;
}

/* -------------------------------------------------------------------------
 * Magnitude Addition & Subtraction
 * ------------------------------------------------------------------------- */

int bigint_add_abs(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    size_t max_size = (a->size > b->size ? a->size : b->size);
    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, max_size + 1)) return 0;

    uint64_t carry = 0;
    for (size_t i = 0; i < max_size; i++) {
        uint64_t sum = carry;
        if (i < a->size) sum += a->digits[i];
        if (i < b->size) sum += b->digits[i];

        tmp.digits[i] = (uint32_t)(sum % BASE);
        carry = sum / BASE;
    }

    size_t res_size = max_size;
    if (carry > 0) {
        tmp.digits[res_size++] = (uint32_t)carry;
    }
    tmp.size = res_size;
    tmp.sign = 1;
    bigint_normalize(&tmp);

    bigint_free(res);
    *res = tmp;
    return 1;
}

int bigint_sub_abs(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;
    /* Assumes |a| >= |b| */

    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, a->size)) return 0;

    int64_t borrow = 0;
    for (size_t i = 0; i < a->size; i++) {
        int64_t diff = (int64_t)a->digits[i] - (i < b->size ? (int64_t)b->digits[i] : 0) - borrow;
        if (diff < 0) {
            diff += BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        tmp.digits[i] = (uint32_t)diff;
    }

    tmp.size = a->size;
    tmp.sign = 1;
    bigint_normalize(&tmp);

    bigint_free(res);
    *res = tmp;
    return 1;
}

/* -------------------------------------------------------------------------
 * Signed Addition & Subtraction
 * ------------------------------------------------------------------------- */

int bigint_add(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    if (bigint_is_zero(a)) return bigint_copy(res, b);
    if (bigint_is_zero(b)) return bigint_copy(res, a);

    if (a->sign == b->sign) {
        int sign = a->sign;
        if (!bigint_add_abs(res, a, b)) return 0;
        res->sign = sign;
        bigint_normalize(res);
        return 1;
    }

    /* Signs differ: compare magnitudes */
    int cmp = bigint_compare_abs(a, b);
    if (cmp == 0) {
        bigint_clear(res);
        return 1;
    }

    if (cmp > 0) {
        int sign = a->sign;
        if (!bigint_sub_abs(res, a, b)) return 0;
        res->sign = sign;
        bigint_normalize(res);
    } else {
        int sign = b->sign;
        if (!bigint_sub_abs(res, b, a)) return 0;
        res->sign = sign;
        bigint_normalize(res);
    }
    return 1;
}

int bigint_sub(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    if (bigint_is_zero(b)) return bigint_copy(res, a);
    if (bigint_is_zero(a)) {
        if (!bigint_copy(res, b)) return 0;
        res->sign = -res->sign;
        return 1;
    }

    /* a - b is equivalent to a + (-b) */
    BigInteger b_neg;
    bigint_init(&b_neg);
    if (!bigint_copy(&b_neg, b)) return 0;
    b_neg.sign = -b_neg.sign;

    int ret = bigint_add(res, a, &b_neg);
    bigint_free(&b_neg);
    return ret;
}

int bigint_add_small(BigInteger *res, const BigInteger *a, int64_t val) {
    BigInteger b_val;
    bigint_init(&b_val);
    if (!bigint_set_i64(&b_val, val)) {
        bigint_free(&b_val);
        return 0;
    }
    int ret = bigint_add(res, a, &b_val);
    bigint_free(&b_val);
    return ret;
}

/* -------------------------------------------------------------------------
 * Multiplication
 * ------------------------------------------------------------------------- */

int bigint_mul_small(BigInteger *res, const BigInteger *a, uint32_t val) {
    if (!res || !a) return 0;

    if (val == 0 || bigint_is_zero(a)) {
        bigint_clear(res);
        return 1;
    }

    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, a->size + 1)) return 0;

    uint64_t carry = 0;
    for (size_t i = 0; i < a->size; i++) {
        uint64_t prod = (uint64_t)a->digits[i] * val + carry;
        tmp.digits[i] = (uint32_t)(prod % BASE);
        carry = prod / BASE;
    }

    size_t tmp_size = a->size;
    if (carry > 0) {
        tmp.digits[tmp_size++] = (uint32_t)carry;
    }

    tmp.size = tmp_size;
    tmp.sign = a->sign;
    bigint_normalize(&tmp);

    bigint_swap(res, &tmp);
    bigint_free(&tmp);
    return 1;
}

int bigint_mul_schoolbook(BigInteger *res, const BigInteger *a, const BigInteger *b) {
    if (!res || !a || !b) return 0;

    if (bigint_is_zero(a) || bigint_is_zero(b)) {
        bigint_clear(res);
        return 1;
    }

    size_t req_cap = a->size + b->size;
    BigInteger tmp;
    if (!bigint_init_capacity(&tmp, req_cap)) return 0;
    tmp.size = req_cap;

    for (size_t i = 0; i < a->size; i++) {
        uint64_t carry = 0;
        uint64_t ai = a->digits[i];

        for (size_t j = 0; j < b->size; j++) {
            uint64_t cur = (uint64_t)tmp.digits[i + j] + ai * (uint64_t)b->digits[j] + carry;
            tmp.digits[i + j] = (uint32_t)(cur % BASE);
            carry = cur / BASE;
        }

        size_t k = i + b->size;
        while (carry > 0) {
            uint64_t cur = (uint64_t)tmp.digits[k] + carry;
            tmp.digits[k] = (uint32_t)(cur % BASE);
            carry = cur / BASE;
            k++;
        }
    }

    tmp.sign = a->sign * b->sign;
    bigint_normalize(&tmp);

    bigint_swap(res, &tmp);
    bigint_free(&tmp);
    return 1;
}

/* -------------------------------------------------------------------------
 * Helper Conversions for Testing
 * ------------------------------------------------------------------------- */

int bigint_set_u64(BigInteger *a, uint64_t val) {
    if (!a) return 0;
    bigint_clear(a);

    if (val == 0) {
        return 1;
    }

    a->sign = 1;
    a->size = 0;

    while (val > 0) {
        if (!bigint_reserve(a, a->size + 1)) return 0;
        a->digits[a->size++] = (uint32_t)(val % BASE);
        val /= BASE;
    }
    bigint_normalize(a);
    return 1;
}

int bigint_set_i64(BigInteger *a, int64_t val) {
    if (!a) return 0;
    if (val == 0) {
        bigint_clear(a);
        return 1;
    }

    int sign = (val < 0 ? -1 : 1);
    uint64_t uval;
    if (val < 0) {
        /* Avoid undefined behavior with INT64_MIN negation */
        uval = (uint64_t)(-(val + 1)) + 1;
    } else {
        uval = (uint64_t)val;
    }

    if (!bigint_set_u64(a, uval)) return 0;
    a->sign = sign;
    return 1;
}

