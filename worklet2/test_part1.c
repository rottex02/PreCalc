#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "bigint.h"

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

void test_normalization_and_zero(void) {
    printf("[Test] Normalization & Canonical Zero Invariants...\n");
    BigInteger a;
    bigint_init(&a);

    /* Canonical zero check */
    TEST_ASSERT(bigint_is_zero(&a) == 1, "Initial bigint must be zero");
    TEST_ASSERT(a.size == 1, "Zero size must be 1");
    TEST_ASSERT(a.digits[0] == 0, "Zero digits[0] must be 0");
    TEST_ASSERT(a.sign == 0, "Zero sign must be 0");

    /* Multiple zero limbs should normalize to 1 zero limb */
    bigint_resize(&a, 5);
    a.sign = 1;
    bigint_normalize(&a);
    TEST_ASSERT(a.size == 1 && a.digits[0] == 0 && a.sign == 0, "5 zero limbs must normalize to canonical zero");

    /* Leading zero limbs should be stripped */
    bigint_resize(&a, 4);
    a.digits[0] = 12345;
    a.digits[1] = 67890;
    a.digits[2] = 0;
    a.digits[3] = 0;
    a.sign = 1;
    bigint_normalize(&a);
    TEST_ASSERT(a.size == 2, "Leading zeroes must be stripped, size should be 2");
    TEST_ASSERT(a.digits[0] == 12345 && a.digits[1] == 67890, "Retained limbs must match");

    bigint_free(&a);
}

void test_comparisons(void) {
    printf("[Test] Comparisons (Abs & Signed)...\n");
    BigInteger a, b;
    bigint_init(&a);
    bigint_init(&b);

    /* Equal zeroes */
    TEST_ASSERT(bigint_compare_abs(&a, &b) == 0, "0 == 0 abs");
    TEST_ASSERT(bigint_compare(&a, &b) == 0, "0 == 0 signed");
    TEST_ASSERT(bigint_equal(&a, &b), "bigint_equal on zeroes");

    /* Size difference (O(1)) */
    bigint_set_u64(&a, 100);
    bigint_set_u64(&b, 10000000000ULL); /* 2 limbs */
    TEST_ASSERT(bigint_compare_abs(&a, &b) < 0, "1 limb < 2 limbs");
    TEST_ASSERT(bigint_compare_abs(&b, &a) > 0, "2 limbs > 1 limb");
    TEST_ASSERT(bigint_less(&a, &b), "a < b");

    /* Same size, different high limb */
    bigint_set_u64(&a, 5000000000ULL); /* high limb 5 */
    bigint_set_u64(&b, 7000000000ULL); /* high limb 7 */
    TEST_ASSERT(bigint_compare_abs(&a, &b) < 0, "5*10^9 < 7*10^9");
    TEST_ASSERT(bigint_compare_abs(&b, &a) > 0, "7*10^9 > 5*10^9");

    /* Signed comparisons */
    a.sign = -1;
    b.sign = 1;
    TEST_ASSERT(bigint_compare(&a, &b) < 0, "-5*10^9 < +7*10^9");
    TEST_ASSERT(bigint_compare(&b, &a) > 0, "+7*10^9 > -5*10^9");

    b.sign = -1;
    /* -5*10^9 vs -7*10^9 => -5*10^9 > -7*10^9 */
    TEST_ASSERT(bigint_compare(&a, &b) > 0, "-5*10^9 > -7*10^9");

    bigint_free(&a);
    bigint_free(&b);
}

void test_addition(void) {
    printf("[Test] Addition (Carries, Signs, Aliasing)...\n");
    BigInteger a, b, res;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&res);

    /* Simple addition */
    bigint_set_u64(&a, 123456789);
    bigint_set_u64(&b, 987654321);
    bigint_add(&res, &a, &b);
    TEST_ASSERT(res.size == 2 && res.digits[0] == 111111110 && res.digits[1] == 1,
                "123456789 + 987654321 = 1111111110 (1 limb overflow into 2nd)");

    /* Cascading carry over multiple limbs: (BASE-1) across 4 limbs + 1 */
    bigint_resize(&a, 4);
    for (size_t i = 0; i < 4; i++) {
        a.digits[i] = BASE - 1;
    }
    a.sign = 1;
    bigint_set_u64(&b, 1);

    bigint_add(&res, &a, &b);
    TEST_ASSERT(res.size == 5, "Result must expand to 5 limbs");
    for (size_t i = 0; i < 4; i++) {
        TEST_ASSERT(res.digits[i] == 0, "Lower 4 limbs must be 0 after cascade");
    }
    TEST_ASSERT(res.digits[4] == 1, "5th limb must be 1");

    /* Signed addition: (+a) + (-b) where a > b */
    bigint_set_i64(&a, 100);
    bigint_set_i64(&b, -40);
    bigint_add(&res, &a, &b);
    TEST_ASSERT(res.sign == 1 && res.digits[0] == 60, "100 + (-40) = 60");

    /* Signed addition: (+a) + (-b) where a < b */
    bigint_set_i64(&a, 40);
    bigint_set_i64(&b, -100);
    bigint_add(&res, &a, &b);
    TEST_ASSERT(res.sign == -1 && res.digits[0] == 60, "40 + (-100) = -60");

    /* Cancellation to exact zero */
    bigint_set_i64(&a, 500);
    bigint_set_i64(&b, -500);
    bigint_add(&res, &a, &b);
    TEST_ASSERT(bigint_is_zero(&res), "500 + (-500) = 0");

    /* Aliasing test: res == a */
    bigint_set_u64(&a, 15);
    bigint_set_u64(&b, 25);
    bigint_add(&a, &a, &b); /* a = a + b */
    TEST_ASSERT(a.digits[0] == 40, "Aliasing a = a + b");

    /* Double self-aliasing: a = a + a */
    bigint_add(&a, &a, &a); /* a = 40 + 40 = 80 */
    TEST_ASSERT(a.digits[0] == 80, "Aliasing a = a + a");

    bigint_free(&a);
    bigint_free(&b);
    bigint_free(&res);
}

void test_subtraction(void) {
    printf("[Test] Subtraction (Borrows, Signs, Aliasing)...\n");
    BigInteger a, b, res;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&res);

    /* Multi-limb borrow: 1 followed by zeroes minus 1 */
    bigint_resize(&a, 4);
    a.digits[0] = 0;
    a.digits[1] = 0;
    a.digits[2] = 0;
    a.digits[3] = 1;
    a.sign = 1;

    bigint_set_u64(&b, 1);
    bigint_sub(&res, &a, &b);

    TEST_ASSERT(res.size == 3, "Size drops from 4 to 3 limbs");
    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT(res.digits[i] == BASE - 1, "Limbs become BASE - 1");
    }

    /* Subtraction leading to negative result: 50 - 150 = -100 */
    bigint_set_i64(&a, 50);
    bigint_set_i64(&b, 150);
    bigint_sub(&res, &a, &b);
    TEST_ASSERT(res.sign == -1 && res.digits[0] == 100, "50 - 150 = -100");

    /* Negative minus negative: -50 - (-150) = 100 */
    bigint_set_i64(&a, -50);
    bigint_set_i64(&b, -150);
    bigint_sub(&res, &a, &b);
    TEST_ASSERT(res.sign == 1 && res.digits[0] == 100, "-50 - (-150) = 100");

    /* Aliasing test: a = a - b */
    bigint_set_u64(&a, 100);
    bigint_set_u64(&b, 30);
    bigint_sub(&a, &a, &b);
    TEST_ASSERT(a.digits[0] == 70, "Aliasing a = a - b");

    /* Self subtraction: a = a - a */
    bigint_sub(&a, &a, &a);
    TEST_ASSERT(bigint_is_zero(&a), "Self subtraction a = a - a = 0");

    bigint_free(&a);
    bigint_free(&b);
    bigint_free(&res);
}

void test_multiplication(void) {
    printf("[Test] Multiplication (Schoolbook, Carries, Aliasing)...\n");
    BigInteger a, b, res;
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&res);

    /* Zero multiplication */
    bigint_set_u64(&a, 123456789);
    bigint_set_u64(&b, 0);
    bigint_mul_schoolbook(&res, &a, &b);
    TEST_ASSERT(bigint_is_zero(&res), "x * 0 = 0");

    /* Multiplication by 1 */
    bigint_set_u64(&b, 1);
    bigint_mul_schoolbook(&res, &a, &b);
    TEST_ASSERT(bigint_equal(&res, &a), "x * 1 = x");

    /* Sign handling */
    a.sign = -1;
    b.sign = 1;
    bigint_mul_schoolbook(&res, &a, &b);
    TEST_ASSERT(res.sign == -1, "(-a) * (+b) has sign -1");

    b.sign = -1;
    bigint_mul_schoolbook(&res, &a, &b);
    TEST_ASSERT(res.sign == 1, "(-a) * (-b) has sign +1");

    /* Multi-limb multiplication */
    /* Let a = 10^9 + 1 (limbs: [1, 1]), b = 10^9 + 2 (limbs: [2, 1]) */
    /* a * b = 10^18 + 3*10^9 + 2 */
    /* Limbs should be: digits[0] = 2, digits[1] = 3, digits[2] = 1 */
    bigint_resize(&a, 2);
    a.digits[0] = 1;
    a.digits[1] = 1;
    a.sign = 1;

    bigint_resize(&b, 2);
    b.digits[0] = 2;
    b.digits[1] = 1;
    b.sign = 1;

    bigint_mul_schoolbook(&res, &a, &b);
    TEST_ASSERT(res.size == 3, "(10^9 + 1)*(10^9 + 2) must have 3 limbs");
    TEST_ASSERT(res.digits[0] == 2, "Limb 0 == 2");
    TEST_ASSERT(res.digits[1] == 3, "Limb 1 == 3");
    TEST_ASSERT(res.digits[2] == 1, "Limb 2 == 1");

    /* Aliasing: a = a * a */
    bigint_set_u64(&a, 1000000);
    bigint_mul_schoolbook(&a, &a, &a); /* 10^6 * 10^6 = 10^12 = 1000 * 10^9 */
    TEST_ASSERT(a.size == 2 && a.digits[0] == 0 && a.digits[1] == 1000,
                "Aliasing a = a * a handles square correctly");

    /* Small multiplication helper */
    bigint_set_u64(&a, 500);
    bigint_mul_small(&res, &a, 20);
    TEST_ASSERT(res.digits[0] == 10000, "500 * 20 = 10000");

    bigint_free(&a);
    bigint_free(&b);
    bigint_free(&res);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 2 / PART 1 UNIT TESTS   \n");
    printf("=========================================\n");

    test_normalization_and_zero();
    test_comparisons();
    test_addition();
    test_subtraction();
    test_multiplication();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}

