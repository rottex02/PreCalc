#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "decimal.h"
#include "complex.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        printf("FAILED: %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

void test_decimal_io(void) {
    printf("[Test] BigDecimal String Parsing & Serialization...\n");
    BigDecimal d;
    decimal_init(&d);

    /* "123.456" -> scale 3, significand 123456 */
    TEST_ASSERT(decimal_from_string(&d, "123.456") == 1, "Parse '123.456'");
    TEST_ASSERT(d.scale == 3, "Scale is 3");
    char *s = decimal_to_string(&d);
    TEST_ASSERT(s && strcmp(s, "123.456") == 0, "Serialize '123.456'");
    free(s);
    decimal_free(&d);

    /* "-0.005" -> scale 3, significand -5 */
    TEST_ASSERT(decimal_from_string(&d, "-0.005") == 1, "Parse '-0.005'");
    TEST_ASSERT(d.scale == 3 && d.significand.sign == -1, "Scale 3, negative");
    s = decimal_to_string(&d);
    TEST_ASSERT(s && strcmp(s, "-0.005") == 0, "Serialize '-0.005'");
    free(s);
    decimal_free(&d);

    /* Exponent: "1.2e3" -> 1200 */
    TEST_ASSERT(decimal_from_string(&d, "1.2e3") == 1, "Parse '1.2e3'");
    s = decimal_to_string(&d);
    TEST_ASSERT(s && strcmp(s, "1200") == 0, "Serialize 1200");
    free(s);
    decimal_free(&d);

    /* Exponent: "1.2e-2" -> 0.012 */
    TEST_ASSERT(decimal_from_string(&d, "1.2e-2") == 1, "Parse '1.2e-2'");
    s = decimal_to_string(&d);
    TEST_ASSERT(s && strcmp(s, "0.012") == 0, "Serialize 0.012");
    free(s);
    decimal_free(&d);
}

void test_decimal_arithmetic(void) {
    printf("[Test] BigDecimal Arithmetic (+, -, *, /, round)...\n");
    BigDecimal a, b, res;
    decimal_init(&a); decimal_init(&b); decimal_init(&res);

    /* Addition: 12.345 + 0.000678 = 12.345678 (Part 6 prompt example) */
    decimal_from_string(&a, "12.345");
    decimal_from_string(&b, "0.000678");
    TEST_ASSERT(decimal_add(&res, &a, &b) == 1, "12.345 + 0.000678");
    char *s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "12.345678") == 0, "Result is '12.345678'");
    free(s);

    /* Subtraction: 100.0 - 0.05 = 99.95 */
    decimal_from_string(&a, "100.0");
    decimal_from_string(&b, "0.05");
    TEST_ASSERT(decimal_sub(&res, &a, &b) == 1, "100.0 - 0.05");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "99.95") == 0, "Result is '99.95'");
    free(s);

    /* Multiplication: 1.25 * 2.5 = 3.125 */
    decimal_from_string(&a, "1.25");
    decimal_from_string(&b, "2.5");
    TEST_ASSERT(decimal_mul(&res, &a, &b) == 1, "1.25 * 2.5");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "3.125") == 0, "Result is '3.125'");
    free(s);

    /* Division: 1 / 3 to 20 decimal places -> 0.33333333333333333333 */
    decimal_from_string(&a, "1");
    decimal_from_string(&b, "3");
    TEST_ASSERT(decimal_div(&res, &a, &b, 20) == 1, "1 / 3 to 20 decimals");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0.33333333333333333333") == 0, "20-digit fraction matches 1/3");
    free(s);

    /* Rounding: 12.34567 rounded to 2 decimals HALF_UP -> 12.35 */
    decimal_from_string(&a, "12.34567");
    decimal_round(&a, 2, ROUND_HALF_UP);
    s = decimal_to_string(&a);
    TEST_ASSERT(s && strcmp(s, "12.35") == 0, "Round 12.34567 to 2 decimals is '12.35'");
    free(s);

    decimal_free(&a); decimal_free(&b); decimal_free(&res);
}

void test_complex_arithmetic(void) {
    printf("[Test] Complex Number Arithmetic (+, -, *, /)...\n");
    Complex a, b, res;
    complex_init(&a); complex_init(&b); complex_init(&res);

    /* (1 + 2i) * (3 + 4i) = -5 + 10i (Part 6 prompt example) */
    complex_set_str(&a, "1", "2");
    complex_set_str(&b, "3", "4");
    TEST_ASSERT(complex_mul(&res, &a, &b) == 1, "Complex mul (1+2i)*(3+4i)");
    char *s = complex_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "-5 + 10i") == 0, "Result matches '-5 + 10i'");
    free(s);

    /* Addition: (1 + 2i) + (3 - 4i) = 4 - 2i */
    complex_set_str(&b, "3", "-4");
    TEST_ASSERT(complex_add(&res, &a, &b) == 1, "Complex add (1+2i)+(3-4i)");
    s = complex_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "4 - 2i") == 0, "Result matches '4 - 2i'");
    free(s);

    /* Division by conjugate: (1 + 2i) / (3 + 4i) = 0.44 + 0.08i */
    /* (1+2i)(3-4i) / (9+16) = (3 - 4i^2 + 2i - 4i^2 ...) -> (3+8) + (6-4)i = 11 + 2i / 25 = 0.44 + 0.08i */
    complex_set_str(&b, "3", "4");
    TEST_ASSERT(complex_div(&res, &a, &b, 2) == 1, "Complex div (1+2i)/(3+4i) with precision 2");
    s = complex_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0.44 + 0.08i") == 0, "Result matches '0.44 + 0.08i'");
    free(s);

    complex_free(&a); complex_free(&b); complex_free(&res);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 3 / PART 6 UNIT TESTS   \n");
    printf("=========================================\n");

    test_decimal_io();
    test_decimal_arithmetic();
    test_complex_arithmetic();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}

