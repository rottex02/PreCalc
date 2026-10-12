#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "eval.h"

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

static char *eval_string_helper(const char *expr, EvalContext *ctx) {
    ParsedExpression pe;
    parsed_expression_init(&pe);

    if (!parse_string_to_ast(expr, &pe)) {
        parsed_expression_free(&pe);
        return NULL;
    }

    Value res;
    value_init(&res, VAL_INT);
    if (!eval_node(pe.root, ctx, &res)) {
        printf("DEBUG eval_node failed for '%s': code=%d msg='%s'\n", expr, ctx->error.code, ctx->error.message);
        value_free(&res);
        parsed_expression_free(&pe);
        return NULL;
    }

    char *s = value_to_string(&res);
    value_free(&res);
    parsed_expression_free(&pe);
    return s;
}

void test_full_expression_evaluation(void) {
    printf("[Test] Full Expression Evaluation with Precedence & Functions...\n");
    SymbolTable st;
    symtab_init(&st);
    symtab_preload_constants(&st, 20);

    EvalContext ctx;
    eval_context_init(&ctx, &st);

    /* Test 1 from Part 8 prompt:
     * "1 + 2*3^2 - (40/8) + gcd(48, 18)"
     * 1 + 2*9 - 5 + 6 = 1 + 18 - 5 + 6 = 20 */
    char *s = eval_string_helper("1 + 2*3^2 - (40/8) + gcd(48, 18)", &ctx);
    TEST_ASSERT(s && strcmp(s, "20") == 0, "1 + 2*3^2 - (40/8) + gcd(48, 18) == 20");
    free(s);

    /* Test 2: Factorial postfix */
    s = eval_string_helper("5! + 2^3", &ctx);
    TEST_ASSERT(s && strcmp(s, "128") == 0, "5! + 2^3 == 128");
    free(s);

    symtab_free(&st);
}

void test_operand_promotion(void) {
    printf("[Test] Automatic Operand Promotion (VAL_INT -> VAL_DEC -> VAL_COMPLEX)...\n");
    SymbolTable st;
    symtab_init(&st);
    symtab_preload_constants(&st, 20);

    EvalContext ctx;
    eval_context_init(&ctx, &st);

    /* Test from Part 8 prompt: "10 + 2.5" == 12.5 */
    char *s = eval_string_helper("10 + 2.5", &ctx);
    TEST_ASSERT(s && strcmp(s, "12.5") == 0, "10 + 2.5 == 12.5");
    free(s);

    /* Decimal multiply integer */
    s = eval_string_helper("2.5 * 4", &ctx);
    TEST_ASSERT(s && (strcmp(s, "10") == 0 || strcmp(s, "10.0") == 0), "2.5 * 4 == 10");
    free(s);

    symtab_free(&st);
}

void test_variables_and_constants(void) {
    printf("[Test] Symbol Table Variable Assignment, Constant Lookup & Trigonometry...\n");
    SymbolTable st;
    symtab_init(&st);
    symtab_preload_constants(&st, 20);

    EvalContext ctx;
    eval_context_init(&ctx, &st);

    /* Store variable x = 10 */
    Value vx;
    value_init(&vx, VAL_INT);
    bigint_set_u64(&vx.as.i, 10);
    TEST_ASSERT(symtab_set(&st, "x", &vx) == 1, "Store variable x = 10");
    value_free(&vx);

    /* Evaluate "x * 2" */
    char *s = eval_string_helper("x * 2", &ctx);
    TEST_ASSERT(s && strcmp(s, "20") == 0, "x * 2 == 20");
    free(s);

    /* Preloaded constant sin(pi / 6) == 0.5 */
    s = eval_string_helper("sin(pi / 6)", &ctx);
    TEST_ASSERT(s && (strcmp(s, "0.5") == 0 ||
                      strcmp(s, "0.50000000000000000000") == 0 ||
                      strcmp(s, "0.49999999999999999999") == 0),
                "sin(pi / 6) == 0.5");
    free(s);

    /* sqrt(4) + abs(-5) == 7 */
    s = eval_string_helper("sqrt(4) + abs(-5)", &ctx);
    TEST_ASSERT(s && (strcmp(s, "7") == 0 ||
                      strcmp(s, "7.00000") == 0 ||
                      strcmp(s, "7.00000000000000000000") == 0),
                "sqrt(4) + abs(-5) == 7");
    free(s);

    symtab_free(&st);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 3 / PART 8 UNIT TESTS   \n");
    printf("=========================================\n");

    test_full_expression_evaluation();
    test_operand_promotion();
    test_variables_and_constants();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
