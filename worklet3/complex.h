#ifndef COMPLEX_H
#define COMPLEX_H

#include "decimal.h"

typedef struct {
    BigDecimal real;
    BigDecimal imag;
} Complex;

/* -------------------------------------------------------------------------
 * Lifecycle & String I/O
 * ------------------------------------------------------------------------- */
void  complex_init(Complex *c);
void  complex_free(Complex *c);
int   complex_copy(Complex *dest, const Complex *src);
void  complex_swap(Complex *a, Complex *b);

int   complex_set(Complex *c, const BigDecimal *real, const BigDecimal *imag);
int   complex_set_str(Complex *c, const char *real_str, const char *imag_str);
char *complex_to_string(const Complex *c);

/* -------------------------------------------------------------------------
 * Complex Arithmetic
 * ------------------------------------------------------------------------- */
int   complex_add(Complex *res, const Complex *a, const Complex *b);
int   complex_sub(Complex *res, const Complex *a, const Complex *b);
int   complex_mul(Complex *res, const Complex *a, const Complex *b);
int   complex_div(Complex *res, const Complex *a, const Complex *b, int precision);
int   complex_is_zero(const Complex *c);

#endif /* COMPLEX_H */

