#define _POSIX_C_SOURCE 200809L
#include "functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        printf("FAILED: %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

void test_constants(void) {
    printf("[Test] High-Precision Mathematical Constants...\n");
    BigDecimal pi, e, ln10;
    decimal_init(&pi); decimal_init(&e); decimal_init(&ln10);

    TEST_ASSERT(math_const_pi(&pi, 20) == 1, "math_const_pi 20 decimals");
    char *s = decimal_to_string(&pi);
    TEST_ASSERT(s && strcmp(s, "3.14159265358979323846") == 0, "pi matches 20 decimal digits");
    free(s);

    TEST_ASSERT(math_const_e(&e, 20) == 1, "math_const_e 20 decimals");
    s = decimal_to_string(&e);
    TEST_ASSERT(s && strcmp(s, "2.71828182845904523536") == 0, "e matches 20 decimal digits");
    free(s);

    TEST_ASSERT(math_const_ln10(&ln10, 20) == 1, "math_const_ln10 20 decimals");
    s = decimal_to_string(&ln10);
    TEST_ASSERT(s && strcmp(s, "2.30258509299404568402") == 0, "ln(10) matches 20 decimal digits");
    free(s);

    decimal_free(&pi); decimal_free(&e); decimal_free(&ln10);
}

void test_sqrt(void) {
    printf("[Test] Newton-Raphson Arbitrary-Precision Square Root...\n");
    BigDecimal two, res;
    decimal_init(&two); decimal_init(&res);

    decimal_from_string(&two, "2");
    /* sqrt(2) to 50 decimal digits:
     * 1.41421356237309504880168872420969807856967187537695 */
    TEST_ASSERT(fn_sqrt(&res, &two, 50) == 1, "fn_sqrt(2, 50)");
    char *s = decimal_to_string(&res);
    const char *expected_sqrt2 =
        "1.41421356237309504880168872420969807856967187537695";
    TEST_ASSERT(s && strcmp(s, expected_sqrt2) == 0, "sqrt(2) to 50 digits matches exact constant");
    free(s);

    /* sqrt(0) == 0, sqrt(4) == 2.0 */
    BigDecimal zero;
    decimal_init(&zero);
    decimal_from_string(&zero, "0");
    TEST_ASSERT(fn_sqrt(&res, &zero, 10) == 1, "sqrt(0)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0") == 0, "sqrt(0) == 0");
    free(s);

    /* sqrt(4) == 2 */
    BigDecimal four;
    decimal_init(&four);
    decimal_from_string(&four, "4");
    TEST_ASSERT(fn_sqrt(&res, &four, 5) == 1, "sqrt(4)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && (strcmp(s, "2") == 0 || strcmp(s, "2.00000") == 0), "sqrt(4) == 2");
    free(s);

    decimal_free(&two); decimal_free(&res); decimal_free(&zero); decimal_free(&four);
}

void test_exp(void) {
    printf("[Test] Taylor Series exp(x) with Integer/Fraction Range Splitting...\n");
    BigDecimal one, res;
    decimal_init(&one); decimal_init(&res);

    decimal_from_string(&one, "1");
    /* exp(1) == e (to 30 decimal digits) */
    TEST_ASSERT(fn_exp(&res, &one, 30) == 1, "fn_exp(1, 30)");
    char *s = decimal_to_string(&res);
    const char *expected_e30 = "2.718281828459045235360287471353";
    TEST_ASSERT(s && strcmp(s, expected_e30) == 0, "exp(1) matches e to 30 digits");
    free(s);

    /* exp(0) == 1 */
    BigDecimal zero;
    decimal_init(&zero);
    decimal_from_string(&zero, "0");
    TEST_ASSERT(fn_exp(&res, &zero, 10) == 1, "fn_exp(0)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "1") == 0, "exp(0) == 1");
    free(s);

    /* exp(-1) == 1/e = 0.367879441171442321595523770161 */
    BigDecimal neg_one;
    decimal_init(&neg_one);
    decimal_from_string(&neg_one, "-1");
    TEST_ASSERT(fn_exp(&res, &neg_one, 30) == 1, "fn_exp(-1, 30)");
    s = decimal_to_string(&res);
    const char *expected_inv_e = "0.367879441171442321595523770161";
    TEST_ASSERT(s && strcmp(s, expected_inv_e) == 0, "exp(-1) matches 1/e to 30 digits");
    free(s);

    decimal_free(&one); decimal_free(&res); decimal_free(&zero); decimal_free(&neg_one);
}

void test_ln(void) {
    printf("[Test] Natural Logarithm ln(x) via Range Reduction...\n");
    BigDecimal one, res;
    decimal_init(&one); decimal_init(&res);

    /* ln(1) == 0 */
    decimal_from_string(&one, "1");
    TEST_ASSERT(fn_ln(&res, &one, 20) == 1, "fn_ln(1)");
    char *s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0") == 0, "ln(1) == 0");
    free(s);

    /* ln(e) == 1 */
    BigDecimal e_val;
    decimal_init(&e_val);
    math_const_e(&e_val, 25);
    TEST_ASSERT(fn_ln(&res, &e_val, 20) == 1, "fn_ln(e)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && (strcmp(s, "1") == 0 || strcmp(s, "1.00000000000000000000") == 0), "ln(e) == 1");
    free(s);

    /* ln(2) = 0.69314718055994530942 */
    BigDecimal two;
    decimal_init(&two);
    decimal_from_string(&two, "2");
    TEST_ASSERT(fn_ln(&res, &two, 20) == 1, "fn_ln(2, 20)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0.69314718055994530942") == 0, "ln(2) matches 20 decimal places");
    free(s);

    decimal_free(&one); decimal_free(&res); decimal_free(&e_val); decimal_free(&two);
}

void test_trig(void) {
    printf("[Test] Trigonometric Functions sin(x) & cos(x)...\n");
    BigDecimal pi, six, pi_over_6, res;
    decimal_init(&pi); decimal_init(&six); decimal_init(&pi_over_6); decimal_init(&res);

    math_const_pi(&pi, 25);
    decimal_from_string(&six, "6");
    decimal_div(&pi_over_6, &pi, &six, 25);

    /* Prompt specification: sin(pi/6) evaluates to 0.50000000000000000000 to 20 decimal places */
    TEST_ASSERT(fn_sin(&res, &pi_over_6, 20) == 1, "fn_sin(pi/6, 20)");
    char *s = decimal_to_string(&res);
    TEST_ASSERT(s && (strcmp(s, "0.5") == 0 || strcmp(s, "0.50000000000000000000") == 0),
                "sin(pi/6) evaluates to 0.5 to 20 decimal places");
    free(s);

    /* cos(0) == 1 */
    BigDecimal zero;
    decimal_init(&zero);
    decimal_from_string(&zero, "0");
    TEST_ASSERT(fn_cos(&res, &zero, 20) == 1, "fn_cos(0)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "1") == 0, "cos(0) == 1");
    free(s);

    /* sin(0) == 0 */
    TEST_ASSERT(fn_sin(&res, &zero, 20) == 1, "fn_sin(0)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && strcmp(s, "0") == 0, "sin(0) == 0");
    free(s);

    /* cos(pi) == -1 */
    TEST_ASSERT(fn_cos(&res, &pi, 20) == 1, "fn_cos(pi)");
    s = decimal_to_string(&res);
    TEST_ASSERT(s && (strcmp(s, "-1") == 0 || strcmp(s, "-1.00000000000000000000") == 0), "cos(pi) == -1");
    free(s);

    decimal_free(&pi); decimal_free(&six); decimal_free(&pi_over_6); decimal_free(&res);
    decimal_free(&zero);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 3 / PART 7 UNIT TESTS   \n");
    printf("=========================================\n");

    test_constants();
    test_sqrt();
    test_exp();
    test_ln();
    test_trig();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
