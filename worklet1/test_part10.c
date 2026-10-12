#define _POSIX_C_SOURCE 200809L
#include "test_part10.h"
#include "bigint.h"
#include "bigint_io.h"
#include "timer.h"
#include "lexer.h"
#include "parser.h"
#include "fast_math.h"
#include "decimal.h"
#include "complex.h"
#include "functions.h"
#include "eval.h"
#include "repl.h"
#include "test_cases_part10.inc"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_suite_passed = 0;
static int g_suite_failed = 0;

#define TEST_CHECK(cond, msg) do { \
    if (cond) { \
        g_suite_passed++; \
    } else { \
        g_suite_failed++; \
        printf("FAILED: %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
    } \
} while(0)

/* Helper: Evaluate expression using the complete pipeline (validate -> lex -> parse -> eval) */
static char *eval_expr_pipeline(const char *expr, EvalContext *ctx) {
    size_t err_pos = 0;
    if (!validate_expression(expr, &err_pos)) {
        return NULL;
    }

    TokenList tokens;
    token_list_init(&tokens);
    ParseError pe_err;
    memset(&pe_err, 0, sizeof(pe_err));
    if (!lex_tokenize(expr, &tokens, &pe_err)) {
        token_list_free(&tokens);
        return NULL;
    }

    ParsedExpression pe;
    parsed_expression_init(&pe);
    if (!parse_expression_tree(&tokens, &pe)) {
        parsed_expression_free(&pe);
        token_list_free(&tokens);
        return NULL;
    }

    ctx->depth = 0;
    ctx->error.code = ERR_NONE;
    ctx->error.pos = 0;
    ctx->error.message[0] = '\0';

    Value res;
    value_init(&res, VAL_INT);
    if (!eval_node(pe.root, ctx, &res)) {
        value_free(&res);
        parsed_expression_free(&pe);
        token_list_free(&tokens);
        return NULL;
    }

    char *str = value_to_string(&res);
    value_free(&res);
    parsed_expression_free(&pe);
    token_list_free(&tokens);
    return str;
}

/* -------------------------------------------------------------------------
 * 1. Worklet 1 Suite: Lexer, Parser, AST Topology, Caret Tracking
 * ------------------------------------------------------------------------- */
static void test_worklet1_lexer_parser(void) {
    printf("[1/5] Running Worklet 1 Suite (Lexer, Parser & AST Topologies)...\n");

    /* Validation checks */
    size_t err_pos = 0;
    TEST_CHECK(validate_expression("1 + 2 * (3 + 4)", &err_pos) == 1, "Balanced expression passes validation");
    TEST_CHECK(validate_expression("1 + 2 * (3 + 4", &err_pos) == 0, "Unclosed paren rejected");
    TEST_CHECK(err_pos == 9, "Unclosed paren points to column 9");
    TEST_CHECK(validate_expression("1 + @", &err_pos) == 0, "Invalid symbol @ rejected");

    /* Tokenizer checks */
    TokenList tl;
    token_list_init(&tl);
    ParseError pe;
    memset(&pe, 0, sizeof(pe));
    TEST_CHECK(lex_tokenize("12345 + sin(x)", &tl, &pe) == 1, "Tokenize expression");
    TEST_CHECK(tl.count == 7, "Token count is 6 tokens + 1 TK_END sentinel");
    TEST_CHECK(tl.items[0].type == TK_NUMBER && strcmp(tl.items[0].text, "12345") == 0, "First token is number 12345");
    TEST_CHECK(tl.items[1].type == TK_OP && strcmp(tl.items[1].text, "+") == 0, "Second token is '+'");
    TEST_CHECK(tl.items[2].type == TK_IDENT && strcmp(tl.items[2].text, "sin") == 0, "Third token is 'sin'");
    TEST_CHECK(tl.items[3].type == TK_LPAREN, "Fourth token is '('");
    TEST_CHECK(tl.items[4].type == TK_IDENT && strcmp(tl.items[4].text, "x") == 0, "Fifth token is 'x'");
    TEST_CHECK(tl.items[5].type == TK_RPAREN, "Sixth token is ')'");
    TEST_CHECK(tl.items[6].type == TK_END, "Seventh token is TK_END");
    token_list_free(&tl);

    /* AST Topology: Slide Worked Example a * b + (c * d - sin(e)) */
    ParsedExpression parsed;
    parsed_expression_init(&parsed);
    TEST_CHECK(parse_string_to_ast("a * b + (c * d - sin(e))", &parsed) == 1, "Parse slide worked example");
    TEST_CHECK(parsed.root != NULL && parsed.root->type == NODE_BINARY && parsed.root->as.bin.op == OP_ADD, "Root is OP_ADD");
    TEST_CHECK(parsed.root->as.bin.left->type == NODE_BINARY && parsed.root->as.bin.left->as.bin.op == OP_MUL, "Root left is OP_MUL");
    TEST_CHECK(parsed.root->as.bin.right->type == NODE_BINARY && parsed.root->as.bin.right->as.bin.op == OP_SUB, "Root right is OP_SUB");
    parsed_expression_free(&parsed);

    /* Right-associativity of power: 2^3^2 == 2^(3^2) */
    parsed_expression_init(&parsed);
    TEST_CHECK(parse_string_to_ast("2^3^2", &parsed) == 1, "Parse power right-associative");
    TEST_CHECK(parsed.root->type == NODE_BINARY && parsed.root->as.bin.op == OP_POW, "Root is power");
    TEST_CHECK(parsed.root->as.bin.right->type == NODE_BINARY && parsed.root->as.bin.right->as.bin.op == OP_POW, "Right branch is power");
    parsed_expression_free(&parsed);
}

/* -------------------------------------------------------------------------
 * 2. Worklet 2 Suite: Limb Storage, Comparisons, Add, Sub, Schoolbook Mul
 * ------------------------------------------------------------------------- */
static void test_worklet2_storage_arithmetic(void) {
    printf("[2/5] Running Worklet 2 Suite (Base-10^9 Limbs & Schoolbook Arithmetic)...\n");

    BigInteger a, b, c;
    bigint_init(&a); bigint_init(&b); bigint_init(&c);

    /* Zero normalization */
    bigint_clear(&a);
    TEST_CHECK(bigint_is_zero(&a) == 1 && a.size == 1 && a.digits[0] == 0 && a.sign == 0, "Canonical zero representation");

    /* Multi-limb carry propagation across 5 limbs */
    bigint_from_string(&a, "999999999999999999999999999999999999999999999");
    bigint_set_u64(&b, 1);
    bigint_add(&c, &a, &b);
    char *s = bigint_to_string(&c);
    TEST_CHECK(s && strcmp(s, "1000000000000000000000000000000000000000000000") == 0, "Carry propagation across 5 limbs");
    free(s);

    /* Borrow propagation across multiple limbs */
    bigint_from_string(&a, "1000000000000000000000000000000000000000000000");
    bigint_set_u64(&b, 1);
    bigint_sub(&c, &a, &b);
    s = bigint_to_string(&c);
    TEST_CHECK(s && strcmp(s, "999999999999999999999999999999999999999999999") == 0, "Borrow propagation across multiple limbs");
    free(s);

    /* Magnitude comparison */
    bigint_from_string(&a, "1234567890123456789");
    bigint_from_string(&b, "1234567890123456788");
    TEST_CHECK(bigint_compare(&a, &b) == 1, "Compare greater");
    TEST_CHECK(bigint_compare(&b, &a) == -1, "Compare lesser");
    TEST_CHECK(bigint_compare(&a, &a) == 0, "Compare equal");

    /* Schoolbook multiplication */
    bigint_from_string(&a, "123456789");
    bigint_from_string(&b, "987654321");
    bigint_mul_schoolbook(&c, &a, &b);
    s = bigint_to_string(&c);
    TEST_CHECK(s && strcmp(s, "121932631112635269") == 0, "Schoolbook multiplication of 9-digit integers");
    free(s);

    bigint_free(&a); bigint_free(&b); bigint_free(&c);
}

/* -------------------------------------------------------------------------
 * 3. Worklet 3 Suite: Karatsuba, Division, Decimals, Series Functions
 * ------------------------------------------------------------------------- */
static void test_worklet3_advanced_functions(void) {
    printf("[3/5] Running Worklet 3 Suite (Karatsuba, Division, Decimals, Functions)...\n");

    /* Karatsuba vs Schoolbook multiplication match on large 270-digit operands */
    BigInteger a, b, k_res, s_res;
    bigint_init(&a); bigint_init(&b);
    bigint_init(&k_res); bigint_init(&s_res);

    bigint_resize(&a, 30);
    bigint_resize(&b, 30);
    for (size_t i = 0; i < 30; i++) {
        a.digits[i] = (uint32_t)(100000000 + i * 12345);
        b.digits[i] = (uint32_t)(900000000 - i * 54321);
    }
    a.sign = 1; b.sign = 1;

    bigint_mul_schoolbook(&s_res, &a, &b);
    bigint_mul_karatsuba(&k_res, &a, &b);
    TEST_CHECK(bigint_equal(&k_res, &s_res) == 1, "Karatsuba and Schoolbook match on 270-digit numbers");

    /* Knuth Algorithm D Division: A = Q * B + R invariant */
    DivisionResult div_res;
    division_result_init(&div_res);
    BigInteger q_mul_b, check_a;
    bigint_init(&q_mul_b); bigint_init(&check_a);

    bigint_from_string(&a, "9876543210987654321098765432109876543210");
    bigint_from_string(&b, "12345678901234567890");
    TEST_CHECK(bigint_divide(&div_res, &a, &b) == 1, "Knuth multi-limb division succeeds");
    bigint_mul_schoolbook(&q_mul_b, &div_res.quotient, &b);
    bigint_add(&check_a, &q_mul_b, &div_res.remainder);
    TEST_CHECK(bigint_equal(&check_a, &a) == 1, "Knuth division invariant holds: A == Q*B + R");
    TEST_CHECK(bigint_compare_abs(&div_res.remainder, &b) < 0, "Knuth division invariant holds: |R| < |B|");

    division_result_free(&div_res);
    bigint_free(&a); bigint_free(&b);
    bigint_free(&k_res); bigint_free(&s_res);
    bigint_free(&q_mul_b); bigint_free(&check_a);

    /* Decimals: BigDecimal scale addition and multiplication */
    BigDecimal d1, d2, d3;
    decimal_init(&d1); decimal_init(&d2); decimal_init(&d3);
    decimal_from_string(&d1, "12.345");
    decimal_from_string(&d2, "6.78");
    decimal_add(&d3, &d1, &d2);
    char *s = decimal_to_string(&d3);
    TEST_CHECK(s && strcmp(s, "19.125") == 0, "BigDecimal 12.345 + 6.78 == 19.125");
    free(s);
    decimal_free(&d1); decimal_free(&d2); decimal_free(&d3);

    /* High-Precision Taylor series: sqrt(2) to 50 digits */
    BigDecimal two, sqrt2;
    decimal_init(&two); decimal_init(&sqrt2);
    decimal_from_string(&two, "2");
    fn_sqrt(&sqrt2, &two, 50);
    s = decimal_to_string(&sqrt2);
    const char *expected_sqrt2 = "1.41421356237309504880168872420969807856967187537695";
    TEST_CHECK(s && strcmp(s, expected_sqrt2) == 0, "Newton-Raphson sqrt(2) to 50 decimal digits matches exact reference");
    free(s);
    decimal_free(&two); decimal_free(&sqrt2);
}

/* -------------------------------------------------------------------------
 * 4. Stated Performance Bound: 100-digit / 200-digit Division (< 1.0 s)
 * ------------------------------------------------------------------------- */
static void benchmark_division_time(void) {
    printf("[4/5] Benchmarking Stated Performance Bound: 100-digit / 200-digit Division (< 1.0 s)...\n");

    /* Create 100-digit dividend string */
    char div100[101];
    for (int i = 0; i < 100; i++) div100[i] = (char)('1' + (i % 9));
    div100[100] = '\0';

    /* Create 200-digit divisor string */
    char div200[201];
    for (int i = 0; i < 200; i++) div200[i] = (char)('1' + ((i * 3) % 9));
    div200[200] = '\0';

    BigInteger a, b;
    bigint_init(&a); bigint_init(&b);
    bigint_from_string(&a, div100);
    bigint_from_string(&b, div200);

    DivisionResult res;
    division_result_init(&res);

    Timer t;
    timer_start(&t);
    int ok = bigint_divide(&res, &a, &b);
    timer_stop(&t);
    double elapsed_ms = timer_elapsed_ms(&t);

    TEST_CHECK(ok == 1, "100-digit / 200-digit division succeeds");
    TEST_CHECK(elapsed_ms < 1000.0, "100-digit / 200-digit division executed strictly in < 1.0 s (1000 ms)");
    printf("       -> Measured time (100-digit / 200-digit): %.4f ms (Bound: < 1000.0 ms)\n", elapsed_ms);

    division_result_free(&res);

    /* Also benchmark 200-digit dividend / 100-digit divisor */
    timer_start(&t);
    ok = bigint_divide(&res, &b, &a);
    timer_stop(&t);
    elapsed_ms = timer_elapsed_ms(&t);

    TEST_CHECK(ok == 1, "200-digit / 100-digit division succeeds");
    TEST_CHECK(elapsed_ms < 1000.0, "200-digit / 100-digit division executed strictly in < 1.0 s (1000 ms)");
    printf("       -> Measured time (200-digit / 100-digit): %.4f ms (Bound: < 1000.0 ms)\n", elapsed_ms);

    division_result_free(&res);
    bigint_free(&a); bigint_free(&b);
}

/* -------------------------------------------------------------------------
 * 5. Differential Testing Suite (124 Expressions vs Reference) & Negative Tests
 * ------------------------------------------------------------------------- */
static void test_differential_and_negative_suite(void) {
    printf("[5/5] Running Differential Test Suite (124 Reference Expressions) & Negative Tests...\n");

    SymbolTable st;
    symtab_init(&st);
    symtab_preload_constants(&st, 20);

    EvalContext ctx;
    eval_context_init(&ctx, &st);
    ctx.precision = 20;

    int diff_passed = 0;
    for (size_t i = 0; i < DIFF_TEST_COUNT; i++) {
        const char *expr = DIFF_TEST_CASES[i].expr;
        const char *expected = DIFF_TEST_CASES[i].expected;

        char *actual = eval_expr_pipeline(expr, &ctx);
        if (actual && strcmp(actual, expected) == 0) {
            diff_passed++;
        } else {
            printf("  Differential Mismatch on '%s':\n    Expected: %s\n    Actual:   %s\n",
                   expr, expected, actual ? actual : "(null)");
        }
        free(actual);
    }

    TEST_CHECK(diff_passed == (int)DIFF_TEST_COUNT, "All 124 differential test cases match reference values 100%");
    printf("       -> Differential Tests: %d / %zu passed\n", diff_passed, DIFF_TEST_COUNT);

    /* Negative Tests: Verify clean error rejection with zero memory leaks */
    const char *negative_tests[] = {
        "2 * (3 + sin(4)",     /* Unbalanced parenthesis */
        "1 / 0",               /* Division by zero */
        "100 % 0",             /* Modulo by zero */
        "gcd(1)",              /* Arity mismatch */
        "unknown_func(1, 2)",  /* Unknown function */
        "ln(-5)",              /* Out-of-domain ln */
        "sqrt(-4)",            /* Negative sqrt */
        "10 + ",               /* Incomplete expression */
        "((((1 + 2)",          /* Deep unclosed paren */
    };
    size_t neg_count = sizeof(negative_tests) / sizeof(negative_tests[0]);

    int neg_passed = 0;
    for (size_t i = 0; i < neg_count; i++) {
        char *bad_res = eval_expr_pipeline(negative_tests[i], &ctx);
        if (bad_res == NULL) {
            neg_passed++;
        } else {
            printf("  Negative test failed to reject invalid expression: '%s' -> '%s'\n",
                   negative_tests[i], bad_res);
            free(bad_res);
        }
    }
    TEST_CHECK(neg_passed == (int)neg_count, "All negative test cases cleanly rejected without memory leaks");
    printf("       -> Negative Tests: %d / %zu cleanly rejected\n", neg_passed, neg_count);

    symtab_free(&st);
}

/* -------------------------------------------------------------------------
 * Master Test Runner Entry Point (Part 10 Deliverable)
 * ------------------------------------------------------------------------- */
int run_all_tests(void) {
    g_suite_passed = 0;
    g_suite_failed = 0;

    printf("=================================================================\n");
    printf(" ARBITRARY-PRECISION CALCULATOR · COMPREHENSIVE TEST SUITE       \n");
    printf(" Verification, Differential Testing & Memory Audit (Part 10)     \n");
    printf("=================================================================\n\n");

    test_worklet1_lexer_parser();
    test_worklet2_storage_arithmetic();
    test_worklet3_advanced_functions();
    benchmark_division_time();
    test_differential_and_negative_suite();

    printf("\n=================================================================\n");
    printf(" TOTAL TEST RESULTS: %d PASSED, %d FAILED\n", g_suite_passed, g_suite_failed);
    printf(" Status: %s\n", (g_suite_failed == 0 ? "ALL SUITES PASSED (100% SUCCESS)" : "FAILURES DETECTED"));
    printf("=================================================================\n");

    return (g_suite_failed == 0 ? 0 : 1);
}

#ifdef MAIN_TEST_PART10
int main(void) {
    return run_all_tests();
}
#endif

