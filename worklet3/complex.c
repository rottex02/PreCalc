#include "complex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void complex_init(Complex *c) {
    if (!c) return;
    decimal_init(&c->real);
    decimal_init(&c->imag);
}

void complex_free(Complex *c) {
    if (!c) return;
    decimal_free(&c->real);
    decimal_free(&c->imag);
}

int complex_copy(Complex *dest, const Complex *src) {
    if (!dest || !src) return 0;
    if (!decimal_copy(&dest->real, &src->real)) return 0;
    if (!decimal_copy(&dest->imag, &src->imag)) return 0;
    return 1;
}

void complex_swap(Complex *a, Complex *b) {
    if (!a || !b || a == b) return;
    decimal_swap(&a->real, &b->real);
    decimal_swap(&a->imag, &b->imag);
}

int complex_set(Complex *c, const BigDecimal *real, const BigDecimal *imag) {
    if (!c || !real || !imag) return 0;
    if (!decimal_copy(&c->real, real)) return 0;
    if (!decimal_copy(&c->imag, imag)) return 0;
    return 1;
}

int complex_set_str(Complex *c, const char *real_str, const char *imag_str) {
    if (!c) return 0;
    if (!decimal_from_string(&c->real, real_str)) return 0;
    if (!decimal_from_string(&c->imag, imag_str)) return 0;
    return 1;
}

int complex_is_zero(const Complex *c) {
    if (!c) return 1;
    return (decimal_is_zero(&c->real) && decimal_is_zero(&c->imag));
}

char *complex_to_string(const Complex *c) {
    if (!c) return NULL;

    char *r_str = decimal_to_string(&c->real);
    char *i_str = decimal_to_string(&c->imag);
    if (!r_str || !i_str) {
        free(r_str);
        free(i_str);
        return NULL;
    }

    int real_zero = decimal_is_zero(&c->real);
    int imag_zero = decimal_is_zero(&c->imag);

    if (imag_zero) {
        free(i_str);
        return r_str;
    }

    if (real_zero) {
        size_t len = strlen(i_str) + 4;
        char *out = (char *)malloc(len);
        if (out) {
            snprintf(out, len, "%si", i_str);
        }
        free(r_str);
        free(i_str);
        return out;
    }

    int imag_neg = (c->imag.significand.sign < 0);
    const char *i_digits = (imag_neg ? i_str + 1 : i_str);

    size_t len = strlen(r_str) + strlen(i_digits) + 8;
    char *out = (char *)malloc(len);
    if (out) {
        if (imag_neg) {
            snprintf(out, len, "%s - %si", r_str, i_digits);
        } else {
            snprintf(out, len, "%s + %si", r_str, i_digits);
        }
    }

    free(r_str);
    free(i_str);
    return out;
}

int complex_add(Complex *res, const Complex *a, const Complex *b) {
    if (!res || !a || !b) return 0;

    Complex tmp;
    complex_init(&tmp);

    if (!decimal_add(&tmp.real, &a->real, &b->real) ||
        !decimal_add(&tmp.imag, &a->imag, &b->imag)) {
        complex_free(&tmp);
        return 0;
    }

    complex_swap(res, &tmp);
    complex_free(&tmp);
    return 1;
}

int complex_sub(Complex *res, const Complex *a, const Complex *b) {
    if (!res || !a || !b) return 0;

    Complex tmp;
    complex_init(&tmp);

    if (!decimal_sub(&tmp.real, &a->real, &b->real) ||
        !decimal_sub(&tmp.imag, &a->imag, &b->imag)) {
        complex_free(&tmp);
        return 0;
    }

    complex_swap(res, &tmp);
    complex_free(&tmp);
    return 1;
}

int complex_mul(Complex *res, const Complex *a, const Complex *b) {
    if (!res || !a || !b) return 0;

    /* (a + bi)(c + di) = (ac - bd) + (ad + bc)i */
    BigDecimal ac, bd, ad, bc;
    decimal_init(&ac); decimal_init(&bd);
    decimal_init(&ad); decimal_init(&bc);

    if (!decimal_mul(&ac, &a->real, &b->real) ||
        !decimal_mul(&bd, &a->imag, &b->imag) ||
        !decimal_mul(&ad, &a->real, &b->imag) ||
        !decimal_mul(&bc, &a->imag, &b->real)) {
        decimal_free(&ac); decimal_free(&bd);
        decimal_free(&ad); decimal_free(&bc);
        return 0;
    }

    Complex tmp;
    complex_init(&tmp);

    if (!decimal_sub(&tmp.real, &ac, &bd) ||
        !decimal_add(&tmp.imag, &ad, &bc)) {
        decimal_free(&ac); decimal_free(&bd);
        decimal_free(&ad); decimal_free(&bc);
        complex_free(&tmp);
        return 0;
    }

    decimal_free(&ac); decimal_free(&bd);
    decimal_free(&ad); decimal_free(&bc);

    complex_swap(res, &tmp);
    complex_free(&tmp);
    return 1;
}

int complex_div(Complex *res, const Complex *a, const Complex *b, int precision) {
    if (!res || !a || !b) return 0;

    /* denom = c^2 + d^2 */
    BigDecimal c2, d2, denom;
    decimal_init(&c2); decimal_init(&d2); decimal_init(&denom);

    if (!decimal_mul(&c2, &b->real, &b->real) ||
        !decimal_mul(&d2, &b->imag, &b->imag) ||
        !decimal_add(&denom, &c2, &d2)) {
        decimal_free(&c2); decimal_free(&d2); decimal_free(&denom);
        return 0;
    }
    decimal_free(&c2);
    decimal_free(&d2);

    if (decimal_is_zero(&denom)) {
        decimal_free(&denom);
        return 0; /* Divide by zero */
    }

    /* num_real = ac + bd */
    BigDecimal ac, bd, num_real;
    decimal_init(&ac); decimal_init(&bd); decimal_init(&num_real);
    if (!decimal_mul(&ac, &a->real, &b->real) ||
        !decimal_mul(&bd, &a->imag, &b->imag) ||
        !decimal_add(&num_real, &ac, &bd)) {
        decimal_free(&ac); decimal_free(&bd); decimal_free(&num_real);
        decimal_free(&denom);
        return 0;
    }
    decimal_free(&ac);
    decimal_free(&bd);

    /* num_imag = bc - ad */
    BigDecimal bc, ad, num_imag;
    decimal_init(&bc); decimal_init(&ad); decimal_init(&num_imag);
    if (!decimal_mul(&bc, &a->imag, &b->real) ||
        !decimal_mul(&ad, &a->real, &b->imag) ||
        !decimal_sub(&num_imag, &bc, &ad)) {
        decimal_free(&bc); decimal_free(&ad); decimal_free(&num_imag);
        decimal_free(&num_real);
        decimal_free(&denom);
        return 0;
    }
    decimal_free(&bc);
    decimal_free(&ad);

    Complex tmp;
    complex_init(&tmp);

    if (!decimal_div(&tmp.real, &num_real, &denom, precision) ||
        !decimal_div(&tmp.imag, &num_imag, &denom, precision)) {
        decimal_free(&num_real);
        decimal_free(&num_imag);
        decimal_free(&denom);
        complex_free(&tmp);
        return 0;
    }

    decimal_free(&num_real);
    decimal_free(&num_imag);
    decimal_free(&denom);

    complex_swap(res, &tmp);
    complex_free(&tmp);
    return 1;
}
