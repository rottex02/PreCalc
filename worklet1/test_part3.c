#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

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

void test_validation(void) {
    printf("[Test] Expression Pre-Validation...\n");
    size_t err_pos = 0;

    /* Valid expressions */
    TEST_ASSERT(validate_expression("1 + 2*3^2 - (40/8) + gcd(48, 18)", &err_pos) == 1,
                "Valid composite arithmetic expression");
    TEST_ASSERT(validate_expression("a * b + (c * d - sin(e))", &err_pos) == 1,
                "Valid variable & function expression");
    TEST_ASSERT(validate_expression("5!", &err_pos) == 1,
                "Valid factorial expression");

    /* Empty input */
    TEST_ASSERT(validate_expression("", &err_pos) == 0 && err_pos == 1,
                "Empty string rejected at col 1");
    TEST_ASSERT(validate_expression("   \t  ", &err_pos) == 0,
                "Whitespace-only rejected");

    /* Unclosed '(' (Slide 16 worked example: > 2 * (3 + sin(4)) */
    /* Column 5 is '(' in "2 * (3 + sin(4)" */
    TEST_ASSERT(validate_expression("2 * (3 + sin(4)", &err_pos) == 0 && err_pos == 5,
                "Unclosed '(' detected at column 5");

    /* Extra ')' */
    /* "((1 + 2)))" -> extra ')' at col 10 */
    TEST_ASSERT(validate_expression("((1 + 2)))", &err_pos) == 0 && err_pos == 10,
                "Unmatched ')' detected at column 10");

    /* Disallowed characters */
    /* "2 + $5" -> '$' at col 5 */
    TEST_ASSERT(validate_expression("2 + $5", &err_pos) == 0 && err_pos == 5,
                "Illegal '$' detected at column 5");
    /* "a # b" -> '#' at col 3 */
    TEST_ASSERT(validate_expression("a # b", &err_pos) == 0 && err_pos == 3,
                "Illegal '#' detected at column 3");
}

void test_tokenization_basic(void) {
    printf("[Test] Tokenizer Basic Streams & Position Tracking...\n");
    TokenList tl;
    ParseError err;

    /* Tokenize: "a * b + (c * d - sin(e))" */
    const char *expr = "a * b + (c * d - sin(e))";
    int ok = lex_tokenize(expr, &tl, &err);
    TEST_ASSERT(ok == 1, "Tokenizing slide example expression");
    TEST_ASSERT(tl.count == 15, "Expected 14 tokens + 1 TK_END sentinel");

    /* Check token sequence and column accuracy */
    /* "a" at col 1 */
    TEST_ASSERT(tl.items[0].type == TK_IDENT && strcmp(tl.items[0].text, "a") == 0 && tl.items[0].pos == 1,
                "Token 0: 'a' at col 1");
    /* "*" at col 3 */
    TEST_ASSERT(tl.items[1].type == TK_OP && strcmp(tl.items[1].text, "*") == 0 && tl.items[1].pos == 3,
                "Token 1: '*' at col 3");
    /* "b" at col 5 */
    TEST_ASSERT(tl.items[2].type == TK_IDENT && strcmp(tl.items[2].text, "b") == 0 && tl.items[2].pos == 5,
                "Token 2: 'b' at col 5");
    /* "+" at col 7 */
    TEST_ASSERT(tl.items[3].type == TK_OP && strcmp(tl.items[3].text, "+") == 0 && tl.items[3].pos == 7,
                "Token 3: '+' at col 7");
    /* "(" at col 9 */
    TEST_ASSERT(tl.items[4].type == TK_LPAREN && tl.items[4].pos == 9,
                "Token 4: '(' at col 9");
    /* "sin" at col 18 */
    TEST_ASSERT(tl.items[9].type == TK_IDENT && strcmp(tl.items[9].text, "sin") == 0 && tl.items[9].pos == 18,
                "Token 9: 'sin' at col 18");
    /* ")" at col 24 */
    TEST_ASSERT(tl.items[13].type == TK_RPAREN && tl.items[13].pos == 24,
                "Token 13: ')' at col 24");
    /* TK_END at col 25 */
    TEST_ASSERT(tl.items[14].type == TK_END && tl.items[14].pos == 25,
                "Token 14: TK_END sentinel at col 25");

    token_list_free(&tl);
}

void test_numbers_and_functions(void) {
    printf("[Test] Numbers (Integers, Decimals, Scientific) & Functions with Commas...\n");
    TokenList tl;
    ParseError err;

    /* Multi-argument function call: "gcd(48, 18)" */
    const char *expr = "gcd(48, 18)";
    TEST_ASSERT(lex_tokenize(expr, &tl, &err) == 1, "Tokenize gcd call");
    TEST_ASSERT(tl.items[0].type == TK_IDENT && strcmp(tl.items[0].text, "gcd") == 0, "Ident 'gcd'");
    TEST_ASSERT(tl.items[1].type == TK_LPAREN, "LPAREN");
    TEST_ASSERT(tl.items[2].type == TK_NUMBER && strcmp(tl.items[2].text, "48") == 0 && tl.items[2].pos == 5,
                "Number '48' at col 5");
    TEST_ASSERT(tl.items[3].type == TK_COMMA && tl.items[3].pos == 7, "Comma at col 7");
    TEST_ASSERT(tl.items[4].type == TK_NUMBER && strcmp(tl.items[4].text, "18") == 0 && tl.items[4].pos == 9,
                "Number '18' at col 9");
    TEST_ASSERT(tl.items[5].type == TK_RPAREN, "RPAREN");
    token_list_free(&tl);

    /* Decimals and scientific notation */
    const char *num_expr = "123.456 + 7.89e-10";
    TEST_ASSERT(lex_tokenize(num_expr, &tl, &err) == 1, "Tokenize decimals & exponents");
    TEST_ASSERT(tl.items[0].type == TK_NUMBER && strcmp(tl.items[0].text, "123.456") == 0,
                "Token '123.456'");
    TEST_ASSERT(tl.items[2].type == TK_NUMBER && strcmp(tl.items[2].text, "7.89e-10") == 0,
                "Token '7.89e-10'");
    token_list_free(&tl);

    /* Illegal character error position */
    const char *bad_expr = "10 + @5";
    int ok = lex_tokenize(bad_expr, &tl, &err);
    TEST_ASSERT(ok == 0, "Lexer rejects illegal character");
    TEST_ASSERT(err.code == ERR_INVALID_CHAR, "Error code ERR_INVALID_CHAR");
    TEST_ASSERT(err.position == 6, "Error position is col 6 for '@'");
    TEST_ASSERT(strstr(err.message, "@") != NULL, "Error message contains '@'");
    token_list_free(&tl);
}

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 1 / PART 3 UNIT TESTS   \n");
    printf("=========================================\n");

    test_validation();
    test_tokenization_basic();
    test_numbers_and_functions();

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0 ? 0 : 1);
}
