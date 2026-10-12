#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "decimal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Mathematical Constants (Computed or accurate to >= 120 decimal digits)
 * ------------------------------------------------------------------------- */
int math_const_pi(BigDecimal *out, int precision);
int math_const_e(BigDecimal *out, int precision);
int math_const_ln10(BigDecimal *out, int precision);

/* -------------------------------------------------------------------------
 * Scientific Functions on BigDecimal (Evaluated to target precision)
 * ------------------------------------------------------------------------- */
/* Square root via arbitrary-precision Newton-Raphson iteration:
 * y_{k+1} = 0.5 * (y_k + x / y_k) */
int fn_sqrt(BigDecimal *out, const BigDecimal *x, int precision);

/* Exponential function e^x via range splitting x = m + f (m integer, 0 <= f < 1)
 * and Taylor series: sum_{k=0}^N f^k / k! */
int fn_exp(BigDecimal *out, const BigDecimal *x, int precision);

/* Natural logarithm ln(x) for x > 0 via range reduction:
 * x = y * 10^k, ln(x) = k*ln(10) + 2 * sum_{k=0}^N (1/(2k+1)) * ((y-1)/(y+1))^(2k+1) */
int fn_ln(BigDecimal *out, const BigDecimal *x, int precision);

/* Trigonometric functions with range reduction modulo 2*pi:
 * sin(x) = sum_{k=0}^N (-1)^k * x^(2k+1) / (2k+1)!
 * cos(x) = sum_{k=0}^N (-1)^k * x^(2k) / (2k)! */
int fn_sin(BigDecimal *out, const BigDecimal *x, int precision);
int fn_cos(BigDecimal *out, const BigDecimal *x, int precision);

#ifdef __cplusplus
}
#endif

#endif /* FUNCTIONS_H */

