#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fast_math.h"
#include "bigint_io.h"
#include "timer.h"

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

void test_division_basic(void) {
    printf("[Test] Division Primitives & Invariant A = Q * B + R...\n");
    BigInteger a, b, q_mul_b, check_a;
    DivisionResult res;
    division_result_init(&res);
    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&q_mul_b);
    bigint_init(&check_a);

    /* 1. Division by zero check */
    bigint_set_u64(&a, 100);
    bigint_set_u64(&b, 0);
    TEST_ASSERT(bigint_divide(&res, &a, &b) == 0, "Division by zero must return 0");

    /* 2. Smaller by larger: 50 / 100 = 0 rem 50 */
    bigint_set_u64(&a, 50);
    bigint_set_u64(&b, 100);
    TEST_ASSERT(bigint_divide(&res, &a, &b) == 1, "Divide smaller by larger");
    TEST_ASSERT(bigint_is_zero(&res.quotient), "Quotient is 0");
    TEST_ASSERT(bigint_equal(&res.remainder, &a), "Remainder is dividend");
    division_result_free(&res);

    /* 3. Single-limb divisor: 1000000000 / 3 = 333333333 rem 1 */
    bigint_from_string(&a, "1000000000");
    bigint_set_u64(&b, 3);
    TEST_ASSERT(bigint_divide(&res, &a, &b) == 1, "Single-limb division");
    TEST_ASSERT(res.quotient.digits[0] == 333333333, "Quotient limb is 333333333");
    TEST_ASSERT(res.remainder.digits[0] == 1, "Remainder is 1");
    division_result_free(&res);

    /* 4. Multi-limb division with invariant check: A = Q * B + R */
    /* Let A = 12345678901234567890, B = 987654321 */
    bigint_from_string(&a, "12345678901234567890");
    bigint_from_string(&b, "987654321");
    TEST_ASSERT(bigint_divide(&res, &a, &b) == 1, "Multi-limb division");

    /* Verify A = Q * B + R */
    bigint_mul_schoolbook(&q_mul_b, &res.quotient, &b);
    bigint_add(&check_a, &q_mul_b, &res.remainder);
    TEST_ASSERT(bigint_equal(&check_a, &a), "Invariant holds: A == Q * B + R");
    TEST_ASSERT(bigint_compare_abs(&res.remainder, &b) < 0, "Invariant holds: |R| < |B|");
    division_result_free(&res);

    /* 5. Knuth Algorithm D multi-limb divisor: B has 2+ limbs */
    /* A has 4 limbs, B has 2 limbs */
    bigint_from_string(&a, "987654321098765432109876543210");
    bigint_from_string(&b, "1234567890123456789");
    TEST_ASSERT(bigint_divide(&res, &a, &b) == 1, "Knuth multi-limb division");

    bigint_mul_schoolbook(&q_mul_b, &res.quotient, &b);
    bigint_add(&check_a, &q_mul_b, &res.remainder);
    TEST_ASSERT(bigint_equal(&check_a, &a), "Knuth invariant holds: A == Q * B + R");
    TEST_ASSERT(bigint_compare_abs(&res.remainder, &b) < 0, "Knuth invariant: |R| < |B|");
    division_result_free(&res);

    bigint_free(&a);
    bigint_free(&b);
    bigint_free(&q_mul_b);
    bigint_free(&check_a);
}

void test_stated_performance_bound(void) {
    printf("[Test] Stated Performance Bound: 100-digit / 200-digit Division (< 1.0 s)...\n");

    /* Create 100-digit dividend string */
    char div100[101];
    for (int i = 0; i < 100; i++) div100[i] = (char)('1' + (i % 9));
    div100[100] = '\0';

    /* Create 200-digit divisor string */
    char div200[201];
    for (int i = 0; i < 200; i++) div200[i] = (char)('1' + ((i * 3) % 9));
    div200[200] = '\0';

    BigInteger a, b;
    bigint_init(&a);
    bigint_init(&b);
    bigint_from_string(&a, div100);
    bigint_from_string(&b, div200);

    DivisionResult res;
    division_result_init(&res);

    Timer t;
    timer_start(&t);

    /* 100-digit / 200-digit division (Slide 3 stated bound requirement) */
    int ok = bigint_divide(&res, &a, &b);
    timer_stop(&t);
    double elapsed_ms = timer_elapsed_ms(&t);

    TEST_ASSERT(ok == 1, "100-digit / 200-digit division succeeds");
    TEST_ASSERT(elapsed_ms < 1000.0, "Execution completed under 1.0 second (1000 ms)");
    printf("       -> Measured time for 100-digit / 200-digit: %.4f ms (Bound: < 1000.0 ms)\n", elapsed_ms);

    division_result_free(&res);

    /* Also test 200-digit dividend / 100-digit divisor */
    timer_start(&t);
    ok = bigint_divide(&res, &b, &a);
    timer_stop(&t);
    elapsed_ms = timer_elapsed_ms(&t);

    TEST_ASSERT(ok == 1, "200-digit / 100-digit division succeeds");
    TEST_ASSERT(elapsed_ms < 1000.0, "200-digit / 100-digit division completed under 1.0 second");
    printf("       -> Measured time for 200-digit / 100-digit: %.4f ms (Bound: < 1000.0 ms)\n", elapsed_ms);

    division_result_free(&res);
    bigint_free(&a);
    bigint_free(&b);
}

void test_karatsuba_multiplication(void) {
    printf("[Test] Karatsuba Fast Multiplication vs Schoolbook Multiplication...\n");

    /* Create 2 large operands (> 30 limbs, well above KARATSUBA_CROSSOVER) */
    BigInteger a, b, res_karatsuba, res_schoolbook;
    bigint_init(&a); bigint_init(&b);
    bigint_init(&res_karatsuba); bigint_init(&res_schoolbook);

    /* 30 limbs = 270 decimal digits */
    bigint_resize(&a, 32);
    bigint_resize(&b, 32);
    for (size_t i = 0; i < 32; i++) {
        a.digits[i] = (uint32_t)(123456789 + i * 1000);
        b.digits[i] = (uint32_t)(987654321 - i * 1000);
    }
    a.sign = 1;
    b.sign = 1;

    /* Compute with Schoolbook */
    bigint_mul_schoolbook(&res_schoolbook, &a, &b);

    /* Compute with Karatsuba */
    bigint_mul_karatsuba(&res_karatsuba, &a, &b);

    TEST_ASSERT(bigint_equal(&res_karatsuba, &res_schoolbook),
                "Karatsuba and Schoolbook products match 100% on 270-digit operands");

    /* Verify selector */
    BigInteger res_sel;
    bigint_init(&res_sel);
    bigint_mul(&res_sel, &a, &b);
    TEST_ASSERT(bigint_equal(&res_sel, &res_schoolbook), "bigint_mul selector matches");

    bigint_free(&a); bigint_free(&b);
    bigint_free(&res_karatsuba); bigint_free(&res_schoolbook);
    bigint_free(&res_sel);
}

void test_number_theory(void) {
    printf("[Test] Number Theory: GCD, Fast Power, Modular Power...\n");
    BigInteger a, b, res;
    bigint_init(&a); bigint_init(&b); bigint_init(&res);

    /* 1. Euclidean GCD */
    /* gcd(48, 18) == 6 */
    bigint_set_u64(&a, 48);
    bigint_set_u64(&b, 18);
    bigint_gcd(&res, &a, &b);
    TEST_ASSERT(res.size == 1 && res.digits[0] == 6, "gcd(48, 18) == 6");

    /* Coprime pairs: gcd(10^9 + 7, 10^9 + 9) == 1 */
    bigint_set_u64(&a, 1000000007);
    bigint_set_u64(&b, 1000000009);
    bigint_gcd(&res, &a, &b);
    TEST_ASSERT(bigint_is_one(&res), "gcd(10^9+7, 10^9+9) == 1");

    /* 2. Fast Power */
    /* 2^10 == 1024 */
    bigint_set_u64(&a, 2);
    bigint_pow(&res, &a, 10);
    TEST_ASSERT(res.size == 1 && res.digits[0] == 1024, "2^10 == 1024");

    /* 3^5 == 243 */
    bigint_set_u64(&a, 3);
    bigint_pow(&res, &a, 5);
    TEST_ASSERT(res.size == 1 && res.digits[0] == 243, "3^5 == 243");

    /* 3. Modular Power: 2^100 mod 1000000007 */
    BigInteger exp, mod;
    bigint_init(&exp); bigint_init(&mod);
    bigint_set_u64(&a, 2);
    bigint_set_u64(&exp, 100);
    bigint_set_u64(&mod, 1000000007);

    bigint_mod_pow(&res, &a, &exp, &mod);
    /* 2^100 mod 1000000007 is 976371285 */
    TEST_ASSERT(res.size == 1 && res.digits[0] == 976371285, "2^100 mod 10^9+7 == 976371285");

    bigint_free(&a); bigint_free(&b); bigint_free(&res);
    bigint_free(&exp); bigint_free(&mod);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 3 / PART 5 UNIT TESTS   \n");
    printf("=========================================\n");

    test_division_basic();
    test_stated_performance_bound();
    test_karatsuba_multiplication();
    test_number_theory();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}

