#ifndef TEST_SUITE_H
#define TEST_SUITE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Runs the entire test suite and returns 0 if all tests pass, non-zero on failure. */
int run_all_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SUITE_H */

