#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "parser.h"

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

void test_slide_worked_example(void) {
    printf("[Test] Slide Worked Example AST Topology: a * b + (c * d - sin(e))...\n");
    ParsedExpression pe;
    parsed_expression_init(&pe);

    int ok = parse_string_to_ast("a * b + (c * d - sin(e))", &pe);
    TEST_ASSERT(ok == 1, "Parse slide worked example succeeds");
    TEST_ASSERT(pe.root != NULL, "Root node is not NULL");

    /* 1. Root: Binary '+' */
    TEST_ASSERT(pe.root->type == NODE_BINARY && pe.root->as.bin.op == OP_ADD, "Root is Binary OP_ADD");

    /* 2. Left child: a * b */
    ExprNode *left = pe.root->as.bin.left;
    TEST_ASSERT(left != NULL && left->type == NODE_BINARY && left->as.bin.op == OP_MUL,
                "Root.left is Binary OP_MUL");
    TEST_ASSERT(left->as.bin.left->type == NODE_VAR && strcmp(left->as.bin.left->as.var_name, "a") == 0,
                "Root.left.left is Var 'a'");
    TEST_ASSERT(left->as.bin.right->type == NODE_VAR && strcmp(left->as.bin.right->as.var_name, "b") == 0,
                "Root.left.right is Var 'b'");

    /* 3. Right child: (c * d - sin(e)) */
    ExprNode *right = pe.root->as.bin.right;
    TEST_ASSERT(right != NULL && right->type == NODE_BINARY && right->as.bin.op == OP_SUB,
                "Root.right is Binary OP_SUB");

    /* 3a. Right.left: c * d */
    ExprNode *c_mul_d = right->as.bin.left;
    TEST_ASSERT(c_mul_d->type == NODE_BINARY && c_mul_d->as.bin.op == OP_MUL, "Right.left is OP_MUL");
    TEST_ASSERT(strcmp(c_mul_d->as.bin.left->as.var_name, "c") == 0, "Right.left.left is Var 'c'");
    TEST_ASSERT(strcmp(c_mul_d->as.bin.right->as.var_name, "d") == 0, "Right.left.right is Var 'd'");

    /* 3b. Right.right: sin(e) */
    ExprNode *sin_call = right->as.bin.right;
    TEST_ASSERT(sin_call->type == NODE_CALL, "Right.right is NODE_CALL");
    TEST_ASSERT(sin_call->as.call.fn_id == func_lookup("sin"), "Function is 'sin'");
    TEST_ASSERT(sin_call->as.call.argc == 1, "sin argc is 1");
    TEST_ASSERT(sin_call->as.call.args[0]->type == NODE_VAR &&
                strcmp(sin_call->as.call.args[0]->as.var_name, "e") == 0, "sin arg 0 is Var 'e'");

    parsed_expression_free(&pe);
}

void test_precedence_and_associativity(void) {
    printf("[Test] Precedence & Right-Associativity...\n");
    ParsedExpression pe;
    parsed_expression_init(&pe);

    /* 1 + 2 * 3 -> Root is OP_ADD, right child is 2 * 3 */
    int ok = parse_string_to_ast("1 + 2 * 3", &pe);
    TEST_ASSERT(ok == 1, "Parse '1 + 2 * 3'");
    TEST_ASSERT(pe.root->type == NODE_BINARY && pe.root->as.bin.op == OP_ADD, "Root is OP_ADD");
    TEST_ASSERT(pe.root->as.bin.right->type == NODE_BINARY && pe.root->as.bin.right->as.bin.op == OP_MUL,
                "Right child is OP_MUL (multiplication binds tighter)");
    parsed_expression_free(&pe);

    /* Right-associativity of power: 2 ^ 3 ^ 4 -> 2 ^ (3 ^ 4) */
    ok = parse_string_to_ast("2 ^ 3 ^ 4", &pe);
    TEST_ASSERT(ok == 1, "Parse '2 ^ 3 ^ 4'");
    TEST_ASSERT(pe.root->type == NODE_BINARY && pe.root->as.bin.op == OP_POW, "Root is OP_POW");
    /* Left is 2, Right is 3 ^ 4 */
    TEST_ASSERT(pe.root->as.bin.left->type == NODE_NUMBER, "Left child is 2");
    TEST_ASSERT(pe.root->as.bin.right->type == NODE_BINARY && pe.root->as.bin.right->as.bin.op == OP_POW,
                "Right child is OP_POW (right-associative)");
    parsed_expression_free(&pe);
}

void test_unary_signs_and_factorial(void) {
    printf("[Test] Positional Unary Sign Disambiguation & Factorial...\n");
    ParsedExpression pe;
    parsed_expression_init(&pe);

    /* -3 * -4 - (-5) */
    int ok = parse_string_to_ast("-3 * -4 - (-5)", &pe);
    TEST_ASSERT(ok == 1, "Parse '-3 * -4 - (-5)'");
    /* Root is binary '-' */
    TEST_ASSERT(pe.root->type == NODE_BINARY && pe.root->as.bin.op == OP_SUB, "Root is binary OP_SUB");

    /* Left child is (-3) * (-4) */
    ExprNode *left = pe.root->as.bin.left;
    TEST_ASSERT(left->type == NODE_BINARY && left->as.bin.op == OP_MUL, "Left child is binary OP_MUL");
    TEST_ASSERT(left->as.bin.left->type == NODE_UNARY && left->as.bin.left->as.un.op == OP_NEG,
                "Left.left is unary OP_NEG");
    TEST_ASSERT(left->as.bin.right->type == NODE_UNARY && left->as.bin.right->as.un.op == OP_NEG,
                "Left.right is unary OP_NEG");

    /* Right child is -(-5) */
    ExprNode *right = pe.root->as.bin.right;
    TEST_ASSERT(right->type == NODE_UNARY && right->as.un.op == OP_NEG, "Right child is unary OP_NEG");
    parsed_expression_free(&pe);

    /* Factorial: 5! */
    ok = parse_string_to_ast("5!", &pe);
    TEST_ASSERT(ok == 1, "Parse '5!'");
    TEST_ASSERT(pe.root->type == NODE_UNARY && pe.root->as.un.op == OP_FACT, "Root is unary OP_FACT");
    parsed_expression_free(&pe);

    /* Factorial precedence with power: 3! ^ 2 */
    ok = parse_string_to_ast("3! ^ 2", &pe);
    TEST_ASSERT(ok == 1, "Parse '3! ^ 2'");
    TEST_ASSERT(pe.root->type == NODE_BINARY && pe.root->as.bin.op == OP_POW, "Root is OP_POW");
    TEST_ASSERT(pe.root->as.bin.left->type == NODE_UNARY && pe.root->as.bin.left->as.un.op == OP_FACT,
                "Left child is OP_FACT (factorial binds tighter than power)");
    parsed_expression_free(&pe);
}

void test_functions_and_errors(void) {
    printf("[Test] Function Calls & Parse-Time Arity Validation...\n");
    ParsedExpression pe;
    parsed_expression_init(&pe);

    /* Valid 2-arg function: gcd(48, 18) */
    int ok = parse_string_to_ast("gcd(48, 18)", &pe);
    TEST_ASSERT(ok == 1, "Parse gcd(48, 18)");
    TEST_ASSERT(pe.root->type == NODE_CALL && pe.root->as.call.argc == 2, "gcd has 2 arguments");
    parsed_expression_free(&pe);

    /* Unknown function */
    ok = parse_string_to_ast("foobar(1)", &pe);
    TEST_ASSERT(ok == 0, "Unknown function 'foobar' rejected");
    TEST_ASSERT(strstr(pe.error.message, "Unknown function") != NULL, "Error message reports unknown function");
    parsed_expression_free(&pe);

    /* Wrong arity for sin (takes 1, got 2) */
    ok = parse_string_to_ast("sin(1, 2)", &pe);
    TEST_ASSERT(ok == 0, "sin with 2 arguments rejected");
    TEST_ASSERT(strstr(pe.error.message, "argument") != NULL, "Error message reports arity error");
    parsed_expression_free(&pe);

    /* Wrong arity for gcd (takes 2, got 1) */
    ok = parse_string_to_ast("gcd(48)", &pe);
    TEST_ASSERT(ok == 0, "gcd with 1 argument rejected");
    parsed_expression_free(&pe);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 1 / PART 4 UNIT TESTS   \n");
    printf("=========================================\n");

    test_slide_worked_example();
    test_precedence_and_associativity();
    test_unary_signs_and_factorial();
    test_functions_and_errors();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}

