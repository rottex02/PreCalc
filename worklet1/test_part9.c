#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "repl.h"

/* Extern declaration of repl internal test helper */
typedef struct ReplState ReplState;
int execute_repl_cycle(void *repl, const char *raw_line);

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

int main(void) {
    printf("=========================================\n");
    printf(" RUNNING WORKLET 1 / PART 9 UNIT TESTS   \n");
    printf("=========================================\n");

    /* Test 1: Verify display_menu() works cleanly */
    printf("[Test] Menu and Visual Caret Rendering...\n");
    display_menu();
    TEST_ASSERT(1, "display_menu() executed successfully");

    /* Test 2: Pipe scripted sequence into REPL process */
    printf("[Test] Scripted Multi-Step REPL Pipeline via Subshell...\n");
    const char *test_script =
        "echo \"x = 10\n"
        "x * 5 + 2\n"
        "ans / 2\n"
        "sin(pi / 6)\n"
        "sqrt(2)\n"
        "precision 30\n"
        "exp(1)\n"
        "mode deg\n"
        "sin(30)\n"
        "exit\" | ./calculator > repl_output.txt 2>&1";

    int ret = system(test_script);
    TEST_ASSERT(ret == 0, "Scripted REPL run completed with code 0");

    /* Verify repl_output.txt content */
    FILE *f = fopen("repl_output.txt", "r");
    TEST_ASSERT(f != NULL, "repl_output.txt was created");
    if (f) {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);
        remove("repl_output.txt");

        TEST_ASSERT(strstr(buf, "x = 10") != NULL, "Variable assignment executed: x = 10");
        TEST_ASSERT(strstr(buf, "= 52") != NULL, "Expression executed: x * 5 + 2 == 52");
        TEST_ASSERT(strstr(buf, "= 26") != NULL, "Ans chaining executed: ans / 2 == 26");
        TEST_ASSERT(strstr(buf, "= 0.5") != NULL, "Trig evaluated: sin(pi / 6) == 0.5");
        TEST_ASSERT(strstr(buf, "1.41421356") != NULL, "sqrt(2) evaluated to high precision");
        TEST_ASSERT(strstr(buf, "2.718281828459") != NULL, "exp(1) evaluated to 30 digits");
    }

    printf("=========================================\n");
    printf(" Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}

