#define _POSIX_C_SOURCE 200809L
#include "test_part10.h"
#include "repl.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--test") == 0 ||
                     strcmp(argv[1], "-t") == 0 ||
                     strcmp(argv[1], "test") == 0)) {
        return run_all_tests();
    }
    return run_repl();
}

