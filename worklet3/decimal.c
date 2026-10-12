#define _POSIX_C_SOURCE 200809L
#include "decimal.h"
#include "bigint_io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static const uint32_t POW10[10] = {
    1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U, 10000000U, 100000000U, 1000000000U
};

int bigint_mul_pow10(BigInteger *res, const BigInteger *a, size_t exp10) {
    if (!res || !a) return 0;
    if (bigint_is_zero(a) || exp10 == 0) {
        return bigint_copy(res, a);
    }

    size_t limb_shift = exp10 / BASE_DIGITS;
    size_t rem_exp = exp10 % BASE_DIGITS;

    BigInteger tmp;
    bigint_init(&tmp);

    if (rem_exp > 0) {
        if (!bigint_mul_small(&tmp, a, POW10[rem_exp])) {
            bigint_free(&tmp);
            return 0;
        }
    } else {
        if (!bigint_copy(&tmp, a)) {
            bigint_free(&tmp);
            return 0;
        }
    }

    if (limb_shift > 0) {
        if (!bigint_shift_limbs(res, &tmp, limb_shift)) {
            bigint_free(&tmp);
            return 0;
        }
    } else {
        bigint_swap(res, &tmp);
    }

    bigint_free(&tmp);
    return 1;
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void decimal_init(BigDecimal *d) {
    if (!d) return;
    bigint_init(&d->significand);
    d->scale = 0;
}

void decimal_free(BigDecimal *d) {
    if (!d) return;
    bigint_free(&d->significand);
    d->scale = 0;
}

int decimal_copy(BigDecimal *dest, const BigDecimal *src) {
    if (!dest || !src) return 0;
    if (!bigint_copy(&dest->significand, &src->significand)) return 0;
    dest->scale = src->scale;
    return 1;
}

void decimal_swap(BigDecimal *a, BigDecimal *b) {
    if (!a || !b || a == b) return;
    bigint_swap(&a->significand, &b->significand);
    int tmp_scale = a->scale;
    a->scale = b->scale;
    b->scale = tmp_scale;
}

void decimal_normalize(BigDecimal *d) {
    if (!d) return;
    bigint_normalize(&d->significand);
    if (bigint_is_zero(&d->significand)) {
        d->scale = 0;
    }
}

int decimal_is_zero(const BigDecimal *d) {
    if (!d) return 1;
    return bigint_is_zero(&d->significand);
}

/* -------------------------------------------------------------------------
 * String Conversions
 * ------------------------------------------------------------------------- */

int decimal_from_string(BigDecimal *d, const char *str) {
    if (!d || !str) return 0;

    d->scale = 0;
    bigint_clear(&d->significand);

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;
    if (!*str) return 0;

    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Build continuous digit string and track fractional scale */
    size_t cap = strlen(str) + 16;
    char *digits_buf = (char *)malloc(cap);
    if (!digits_buf) return 0;

    size_t dpos = 0;
    int saw_dot = 0;
    int scale = 0;

    while (*str && (isdigit((unsigned char)*str) || *str == '.')) {
        if (*str == '.') {
            if (saw_dot) {
                free(digits_buf);
                return 0; /* Multiple dots */
            }
            saw_dot = 1;
            str++;
            continue;
        }

        digits_buf[dpos++] = *str;
        if (saw_dot) {
            scale++;
        }
        str++;
    }

    if (dpos == 0) {
        free(digits_buf);
        return 0; /* No digits */
    }
    digits_buf[dpos] = '\0';

    /* Check for exponent [eE][+-]?[0-9]+ */
    int exp_val = 0;
    if (*str == 'e' || *str == 'E') {
        str++;
        int exp_sign = 1;
        if (*str == '-') {
            exp_sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }
        if (!isdigit((unsigned char)*str)) {
            free(digits_buf);
            return 0;
        }
        while (isdigit((unsigned char)*str)) {
            exp_val = exp_val * 10 + (*str - '0');
            str++;
        }
        exp_val *= exp_sign;
    }

    /* Load into BigInteger significand */
    if (!bigint_from_string(&d->significand, digits_buf)) {
        free(digits_buf);
        return 0;
    }
    free(digits_buf);

    d->significand.sign = (bigint_is_zero(&d->significand) ? 0 : sign);

    /* Adjust scale for exponent */
    int final_scale = scale - exp_val;
    if (final_scale < 0) {
        /* Multiply significand by 10^(-final_scale) */
        if (!bigint_mul_pow10(&d->significand, &d->significand, (size_t)(-final_scale))) {
            return 0;
        }
        d->scale = 0;
    } else {
        d->scale = final_scale;
    }

    decimal_normalize(d);
    return 1;
}

char *decimal_to_string(const BigDecimal *d) {
    if (!d) return NULL;

    if (decimal_is_zero(d)) {
        char *z = (char *)malloc(2);
        if (z) { z[0] = '0'; z[1] = '\0'; }
        return z;
    }

    char *raw = bigint_to_string(&d->significand);
    if (!raw) return NULL;

    if (d->scale == 0) {
        return raw;
    }

    int is_neg = (d->significand.sign < 0);
    const char *digits = (is_neg ? raw + 1 : raw);
    size_t len = strlen(digits);

    size_t scale = (size_t)d->scale;
    size_t out_cap = len + scale + 8;
    char *out = (char *)malloc(out_cap);
    if (!out) {
        free(raw);
        return NULL;
    }

    size_t pos = 0;
    if (is_neg) {
        out[pos++] = '-';
    }

    if (len <= scale) {
        out[pos++] = '0';
        out[pos++] = '.';
        size_t pad_zeros = scale - len;
        for (size_t i = 0; i < pad_zeros; i++) {
            out[pos++] = '0';
        }
        memcpy(out + pos, digits, len);
        pos += len;
    } else {
        size_t int_digits = len - scale;
        memcpy(out + pos, digits, int_digits);
        pos += int_digits;
        out[pos++] = '.';
        memcpy(out + pos, digits + int_digits, scale);
        pos += scale;
    }

    out[pos] = '\0';
    free(raw);
    return out;
}

/* -------------------------------------------------------------------------
 * Basic Arithmetic
 * ------------------------------------------------------------------------- */

int decimal_add(BigDecimal *res, const BigDecimal *a, const BigDecimal *b) {
    if (!res || !a || !b) return 0;

    int max_scale = (a->scale > b->scale ? a->scale : b->scale);

    BigInteger a_scaled, b_scaled;
    bigint_init(&a_scaled); bigint_init(&b_scaled);

    if (!bigint_mul_pow10(&a_scaled, &a->significand, (size_t)(max_scale - a->scale)) ||
        !bigint_mul_pow10(&b_scaled, &b->significand, (size_t)(max_scale - b->scale))) {
        bigint_free(&a_scaled); bigint_free(&b_scaled);
        return 0;
    }

    BigDecimal tmp;
    decimal_init(&tmp);

    if (!bigint_add(&tmp.significand, &a_scaled, &b_scaled)) {
        bigint_free(&a_scaled); bigint_free(&b_scaled);
        decimal_free(&tmp);
        return 0;
    }

    tmp.scale = max_scale;
    decimal_normalize(&tmp);

    decimal_swap(res, &tmp);
    decimal_free(&tmp);
    bigint_free(&a_scaled);
    bigint_free(&b_scaled);
    return 1;
}

int decimal_sub(BigDecimal *res, const BigDecimal *a, const BigDecimal *b) {
    if (!res || !a || !b) return 0;

    int max_scale = (a->scale > b->scale ? a->scale : b->scale);

    BigInteger a_scaled, b_scaled;
    bigint_init(&a_scaled); bigint_init(&b_scaled);

    if (!bigint_mul_pow10(&a_scaled, &a->significand, (size_t)(max_scale - a->scale)) ||
        !bigint_mul_pow10(&b_scaled, &b->significand, (size_t)(max_scale - b->scale))) {
        bigint_free(&a_scaled); bigint_free(&b_scaled);
        return 0;
    }

    BigDecimal tmp;
    decimal_init(&tmp);

    if (!bigint_sub(&tmp.significand, &a_scaled, &b_scaled)) {
        bigint_free(&a_scaled); bigint_free(&b_scaled);
        decimal_free(&tmp);
        return 0;
    }

    tmp.scale = max_scale;
    decimal_normalize(&tmp);

    decimal_swap(res, &tmp);
    decimal_free(&tmp);
    bigint_free(&a_scaled);
    bigint_free(&b_scaled);
    return 1;
}

int decimal_mul(BigDecimal *res, const BigDecimal *a, const BigDecimal *b) {
    if (!res || !a || !b) return 0;

    BigDecimal tmp;
    decimal_init(&tmp);

    if (!bigint_mul(&tmp.significand, &a->significand, &b->significand)) {
        decimal_free(&tmp);
        return 0;
    }

    tmp.scale = a->scale + b->scale;
    decimal_normalize(&tmp);

    decimal_swap(res, &tmp);
    decimal_free(&tmp);
    return 1;
}

int decimal_div(BigDecimal *res, const BigDecimal *a, const BigDecimal *b, int precision) {
    if (!res || !a || !b) return 0;
    if (decimal_is_zero(b)) return 0; /* Divide by zero */

    if (precision < 0) precision = 0;

    /* shift = b->scale - a->scale + precision */
    int shift = b->scale - a->scale + precision;

    BigInteger num, den;
    bigint_init(&num); bigint_init(&den);

    if (shift >= 0) {
        if (!bigint_mul_pow10(&num, &a->significand, (size_t)shift) ||
            !bigint_copy(&den, &b->significand)) {
            bigint_free(&num); bigint_free(&den);
            return 0;
        }
    } else {
        if (!bigint_copy(&num, &a->significand) ||
            !bigint_mul_pow10(&den, &b->significand, (size_t)(-shift))) {
            bigint_free(&num); bigint_free(&den);
            return 0;
        }
    }

    DivisionResult div_res;
    division_result_init(&div_res);

    if (!bigint_divide(&div_res, &num, &den)) {
        bigint_free(&num); bigint_free(&den);
        division_result_free(&div_res);
        return 0;
    }

    BigDecimal tmp;
    decimal_init(&tmp);
    bigint_swap(&tmp.significand, &div_res.quotient);
    tmp.scale = precision;
    decimal_normalize(&tmp);

    decimal_swap(res, &tmp);

    decimal_free(&tmp);
    division_result_free(&div_res);
    bigint_free(&num);
    bigint_free(&den);
    return 1;
}

int decimal_round(BigDecimal *d, int target_scale, RoundingMode mode) {
    if (!d || target_scale < 0) return 0;
    if (d->scale <= target_scale) return 1;

    size_t delta = (size_t)(d->scale - target_scale);

    BigInteger divisor;
    bigint_init(&divisor);
    BigInteger one;
    bigint_init(&one);
    bigint_set_u64(&one, 1);
    bigint_mul_pow10(&divisor, &one, delta);
    bigint_free(&one);

    DivisionResult div_res;
    division_result_init(&div_res);

    if (!bigint_divide(&div_res, &d->significand, &divisor)) {
        bigint_free(&divisor);
        division_result_free(&div_res);
        return 0;
    }

    if (mode == ROUND_HALF_UP && !bigint_is_zero(&div_res.remainder)) {
        /* Check if |remainder| * 2 >= divisor */
        BigInteger rem_times_2;
        bigint_init(&rem_times_2);
        bigint_mul_small(&rem_times_2, &div_res.remainder, 2);

        if (bigint_compare_abs(&rem_times_2, &divisor) >= 0) {
            if (d->significand.sign >= 0) {
                bigint_add_small(&div_res.quotient, &div_res.quotient, 1);
            } else {
                bigint_add_small(&div_res.quotient, &div_res.quotient, -1);
            }
        }
        bigint_free(&rem_times_2);
    }

    bigint_swap(&d->significand, &div_res.quotient);
    d->scale = target_scale;
    decimal_normalize(d);

    bigint_free(&divisor);
    division_result_free(&div_res);
    return 1;
}

int decimal_compare(const BigDecimal *a, const BigDecimal *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    int max_scale = (a->scale > b->scale ? a->scale : b->scale);
    BigInteger sa, sb;
    bigint_init(&sa); bigint_init(&sb);

    bigint_mul_pow10(&sa, &a->significand, (size_t)(max_scale - a->scale));
    bigint_mul_pow10(&sb, &b->significand, (size_t)(max_scale - b->scale));

    int cmp = bigint_compare(&sa, &sb);
    bigint_free(&sa);
    bigint_free(&sb);
    return cmp;
}
