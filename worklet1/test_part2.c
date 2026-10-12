#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bigint.h"
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

void test_string_to_bigint(void) {
    printf("[Test] String -> BigInteger Parsing...\n");
    BigInteger a;
    bigint_init(&a);

    /* Zero variants */
    TEST_ASSERT(bigint_from_string(&a, "0") && bigint_is_zero(&a), "Parse '0'");
    TEST_ASSERT(bigint_from_string(&a, "+0") && bigint_is_zero(&a), "Parse '+0'");
    TEST_ASSERT(bigint_from_string(&a, "-0") && bigint_is_zero(&a), "Parse '-0'");
    TEST_ASSERT(bigint_from_string(&a, "00000000") && bigint_is_zero(&a), "Parse '00000000'");

    /* Single limb numbers */
    TEST_ASSERT(bigint_from_string(&a, "123456789") && a.size == 1 && a.digits[0] == 123456789 && a.sign == 1,
                "Parse '123456789'");
    TEST_ASSERT(bigint_from_string(&a, "-987654321") && a.size == 1 && a.digits[0] == 987654321 && a.sign == -1,
                "Parse '-987654321'");

    /* Numbers with leading spaces and leading zeroes */
    TEST_ASSERT(bigint_from_string(&a, "   00042") && a.size == 1 && a.digits[0] == 42,
                "Parse '   00042'");

    /* Multi-limb number: Slide example 12345678901234567890 */
    /* Grouped: [12] [345678901] [234567890] */
    TEST_ASSERT(bigint_from_string(&a, "12345678901234567890"), "Parse Slide 6 example");
    TEST_ASSERT(a.size == 3, "Size must be 3 limbs");
    TEST_ASSERT(a.digits[0] == 234567890, "Limb 0 == 234567890");
    TEST_ASSERT(a.digits[1] == 345678901, "Limb 1 == 345678901");
    TEST_ASSERT(a.digits[2] == 12, "Limb 2 == 12");
    TEST_ASSERT(a.sign == 1, "Sign == +1");

    /* Invalid string inputs */
    TEST_ASSERT(bigint_from_string(&a, "") == 0, "Empty string rejected");
    TEST_ASSERT(bigint_from_string(&a, "   ") == 0, "Whitespace only rejected");
    TEST_ASSERT(bigint_from_string(&a, "12a34") == 0, "Letters rejected");
    TEST_ASSERT(bigint_from_string(&a, "++12") == 0, "Double sign rejected");
    TEST_ASSERT(bigint_from_string(&a, "--12") == 0, "Double minus rejected");
    TEST_ASSERT(bigint_from_string(&a, "+") == 0, "Single plus rejected");
    TEST_ASSERT(bigint_from_string(&a, "-") == 0, "Single minus rejected");

    bigint_free(&a);
}

void test_bigint_to_string(void) {
    printf("[Test] BigInteger -> String Formatting...\n");
    BigInteger a;
    bigint_init(&a);

    /* Zero */
    char *s = bigint_to_string(&a);
    TEST_ASSERT(s && strcmp(s, "0") == 0, "Zero formats to '0'");
    free(s);

    /* Positive single limb */
    bigint_set_u64(&a, 42);
    s = bigint_to_string(&a);
    TEST_ASSERT(s && strcmp(s, "42") == 0, "42 formats to '42'");
    free(s);

    /* Negative single limb */
    bigint_set_i64(&a, -123456);
    s = bigint_to_string(&a);
    TEST_ASSERT(s && strcmp(s, "-123456") == 0, "-123456 formats to '-123456'");
    free(s);

    /* Multi-limb with zero-padded inner limb */
    /* e.g., 1000000001 (1*10^9 + 1 -> digits[1]=1, digits[0]=1) */
    bigint_resize(&a, 2);
    a.digits[0] = 1;
    a.digits[1] = 1;
    a.sign = 1;
    s = bigint_to_string(&a);
    TEST_ASSERT(s && strcmp(s, "1000000001") == 0, "Inner limb properly padded with 8 zeroes");
    free(s);

    bigint_free(&a);
}

void test_round_trip(void) {
    printf("[Test] Round-Trip Conversions (String -> BigInt -> String)...\n");
    const char *test_cases[] = {
        "0",
        "7",
        "-7",
        "1000000000",
        "-1000000000",
        "12345678901234567890",
        "-12345678901234567890",
        "99999999999999999999999999999999999999999999999999", /* 50 digits of 9 */
        "100000000000000000000000000000000000000000000000001",
        /* 200-digit number */
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "9876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210"
    };

    BigInteger a;
    bigint_init(&a);

    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        const char *input = test_cases[i];
        int ok = bigint_from_string(&a, input);
        TEST_ASSERT(ok == 1, "Parsing round-trip input");

        char *output = bigint_to_string(&a);
        TEST_ASSERT(output != NULL, "Serializing round-trip input");
        TEST_ASSERT(strcmp(input, output) == 0, "Round trip strings match exactly");

        free(output);
    }

    bigint_free(&a);
}

void test_timer_functionality(void) {
    printf("[Test] High-Resolution Monotonic Timer...\n");
    Timer t;
    timer_start(&t);

    /* Busy wait for ~10ms */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 15000000L; /* 15 ms */
    nanosleep(&ts, NULL);

    timer_stop(&t);
    double elapsed = timer_elapsed_ms(&t);

    TEST_ASSERT(elapsed >= 10.0 && elapsed <= 100.0, "Timer measures elapsed ~15ms accurately");
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 1 / PART 2 UNIT TESTS   \n");
    printf("=========================================\n");

    test_string_to_bigint();
    test_bigint_to_string();
    test_round_trip();
    test_timer_functionality();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}
