#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "timer.h"
#include "bigint_io.h"
#include "lexer.h"
#include "parser.h"
#include "eval.h"

/* -------------------------------------------------------------------------
 * REPL State & Control
 * ------------------------------------------------------------------------- */
typedef struct {
    int          running;
    SymbolTable  vars;
    EvalContext  ctx;
} ReplState;

static void repl_state_init(ReplState *repl) {
    repl->running = 1;
    symtab_init(&repl->vars);
    eval_context_init(&repl->ctx, &repl->vars);
    repl->ctx.precision = 20;
    repl->ctx.angle_mode = ANGLE_RADIAN;
    repl->ctx.rounding = ROUND_HALF_UP;
    symtab_preload_constants(&repl->vars, repl->ctx.precision);
}

static void repl_state_free(ReplState *repl) {
    symtab_free(&repl->vars);
}

/* -------------------------------------------------------------------------
 * Step 1: display_menu() — Prompt user
 * ------------------------------------------------------------------------- */
void display_menu(void) {
    printf("> ");
    fflush(stdout);
}

/* -------------------------------------------------------------------------
 * Print visual caret error indicator
 * ------------------------------------------------------------------------- */
static void print_error_caret(const char *line, size_t pos, const char *msg) {
    printf("  %s\n  ", line);
    size_t col = (pos > 0 ? pos - 1 : 0);
    for (size_t i = 0; i < col; i++) {
        putchar(' ');
    }
    printf("^\n");
    printf("  %s\n\n", msg);
}

/* -------------------------------------------------------------------------
 * Step 2 & 3: get_operation_choice() & command checks
 * Returns: 1 if line was a command (handled), 0 if expression to evaluate
 * ------------------------------------------------------------------------- */
int handle_repl_command(ReplState *repl, const char *line) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (!*line) return 1; /* Empty line, skip */

    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
        repl->running = 0;
        return 1;
    }

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        printf("\n=== Precision Calculator Commands & Syntax ===\n");
        printf("  Commands:\n");
        printf("    exit, quit         Exit calculator\n");
        printf("    help               Show this help text\n");
        printf("    mode [rad|deg]     Toggle or set angle mode (current: %s)\n",
               repl->ctx.angle_mode == ANGLE_RADIAN ? "radian" : "degree");
        printf("    precision <n>      Set decimal evaluation precision (current: %d)\n",
               repl->ctx.precision);
        printf("    vars               Display all stored variables\n");
        printf("  Operators & Syntax:\n");
        printf("    + - * / %% ^ !     Standard arithmetic, power, modulo, factorial\n");
        printf("    sin, cos, exp, ln  Taylor-series scientific functions\n");
        printf("    sqrt, abs, gcd     Newton-Raphson sqrt, absolute value, Euclidean GCD\n");
        printf("    x = <expr>         Variable assignment\n");
        printf("    pi, e, ans         Preloaded mathematical constants & previous result\n\n");
        return 1;
    }

    if (strncmp(line, "precision", 9) == 0) {
        const char *p = line + 9;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) {
            int new_prec = atoi(p);
            if (new_prec >= 0 && new_prec <= 2000) {
                repl->ctx.precision = new_prec;
                symtab_preload_constants(&repl->vars, new_prec);
                printf("  [Precision set to %d decimal places]\n\n", new_prec);
            } else {
                printf("  [Error: precision must be between 0 and 2000]\n\n");
            }
        } else {
            printf("  [Current precision: %d]\n\n", repl->ctx.precision);
        }
        return 1;
    }

    if (strncmp(line, "mode", 4) == 0) {
        const char *p = line + 4;
        while (*p && isspace((unsigned char)*p)) p++;
        if (strcmp(p, "deg") == 0 || strcmp(p, "degree") == 0) {
            repl->ctx.angle_mode = ANGLE_DEGREE;
            printf("  [Angle mode set to DEGREE]\n\n");
        } else if (strcmp(p, "rad") == 0 || strcmp(p, "radian") == 0) {
            repl->ctx.angle_mode = ANGLE_RADIAN;
            printf("  [Angle mode set to RADIAN]\n\n");
        } else {
            /* Toggle */
            repl->ctx.angle_mode = (repl->ctx.angle_mode == ANGLE_RADIAN) ? ANGLE_DEGREE : ANGLE_RADIAN;
            printf("  [Angle mode switched to %s]\n\n",
                   repl->ctx.angle_mode == ANGLE_RADIAN ? "RADIAN" : "DEGREE");
        }
        return 1;
    }

    if (strcmp(line, "vars") == 0) {
        printf("\n=== Symbol Table Variables ===\n");
        for (size_t i = 0; i < repl->vars.count; i++) {
            char *vstr = value_to_string(&repl->vars.symbols[i].value);
            printf("  %s = %s\n", repl->vars.symbols[i].name, vstr ? vstr : "?");
            free(vstr);
        }
        printf("\n");
        return 1;
    }

    return 0; /* Not a REPL command */
}

/* -------------------------------------------------------------------------
 * Process Variable Assignment: "var_name = expr"
 * ------------------------------------------------------------------------- */
static int check_and_strip_assignment(char *line, char **var_name, char **expr_part) {
    char *eq = strchr(line, '=');
    if (!eq) return 0;

    /* Check if left of '=' is a valid single identifier */
    char *left = line;
    while (*left && isspace((unsigned char)*left)) left++;
    char *end_ident = left;
    if (!isalpha((unsigned char)*end_ident) && *end_ident != '_') return 0;
    while (*end_ident && (isalnum((unsigned char)*end_ident) || *end_ident == '_')) {
        end_ident++;
    }
    char *check_space = end_ident;
    while (*check_space && isspace((unsigned char)*check_space)) check_space++;
    if (check_space != eq) return 0; /* Complex left hand side */

    size_t id_len = (size_t)(end_ident - left);
    char *name = (char *)malloc(id_len + 1);
    if (!name) return 0;
    memcpy(name, left, id_len);
    name[id_len] = '\0';

    *var_name = name;
    *expr_part = eq + 1;
    return 1;
}

/* -------------------------------------------------------------------------
 * The 17-Step Execution Cycle per input line
 * ------------------------------------------------------------------------- */
int execute_repl_cycle(ReplState *repl, const char *raw_line) {
    if (!repl || !raw_line) return 0;

    char *line_copy = strdup(raw_line);
    if (!line_copy) return 0;
    trim_whitespace(line_copy);

    /* Step 2 & 3: Command check & exit */
    if (handle_repl_command(repl, line_copy)) {
        free(line_copy);
        return 1;
    }

    /* Check for variable assignment: "name = expr" */
    char *assign_var = NULL;
    char *eval_expr = line_copy;
    if (check_and_strip_assignment(line_copy, &assign_var, &eval_expr)) {
        trim_whitespace(eval_expr);
    }

    /* Step 5: validate_expression() */
    size_t err_pos = 0;
    if (!validate_expression(eval_expr, &err_pos)) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "ParseError: Invalid syntax or unbalanced parenthesis at column %zu", err_pos);
        print_error_caret(eval_expr, err_pos, err_msg);
        free(assign_var);
        free(line_copy);
        return 0;
    }

    /* Step 6: lex_tokenize(&tokens) */
    TokenList tokens;
    token_list_init(&tokens);
    ParseError parse_err;
    memset(&parse_err, 0, sizeof(parse_err));

    if (!lex_tokenize(eval_expr, &tokens, &parse_err)) {
        print_error_caret(eval_expr, parse_err.position, parse_err.message);
        token_list_free(&tokens);
        free(assign_var);
        free(line_copy);
        return 0;
    }

    /* Step 7 & 8: parse_expression(&tokens) -> ExprNode AST root */
    ParsedExpression pe;
    parsed_expression_init(&pe);
    if (!parse_expression_tree(&tokens, &pe)) {
        /* Step 8: Reject on parse error with visual caret */
        print_error_caret(eval_expr, pe.error.position, pe.error.message);
        parsed_expression_free(&pe);
        token_list_free(&tokens);
        free(assign_var);
        free(line_copy);
        return 0;
    }

    /* Step 9: eval_context_init(&ctx) */
    repl->ctx.depth = 0;
    repl->ctx.error.code = ERR_NONE;
    repl->ctx.error.pos = 0;
    repl->ctx.error.message[0] = '\0';

    /* Step 10: timer_start(&timer) — START TIMED WINDOW */
    Timer timer;
    timer_start(&timer);

    /* Step 11: eval_node(root, &ctx) — Post-order AST evaluation */
    Value result;
    value_init(&result, VAL_INT);
    int eval_ok = eval_node(pe.root, &repl->ctx, &result);

    /* Step 12: timer_stop(&timer) — STOP TIMED WINDOW */
    timer_stop(&timer);

    /* Step 13: Handle eval failure */
    if (!eval_ok) {
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg), "RuntimeError: %s (at col %zu)",
                 repl->ctx.error.message[0] ? repl->ctx.error.message : "Evaluation failed",
                 repl->ctx.error.pos);
        print_error_caret(eval_expr, repl->ctx.error.pos, err_msg);
        value_free(&result);
        parsed_expression_free(&pe);
        token_list_free(&tokens);
        free(assign_var);
        free(line_copy);
        return 0;
    }

    /* Step 14: value_to_string(&result) */
    char *res_str = value_to_string(&result);

    /* Step 15: Display exact digits and timer_elapsed_ms(&timer) */
    if (assign_var) {
        printf("  %s = %s  [%.4f ms]\n\n", assign_var, res_str ? res_str : "0", timer_elapsed_ms(&timer));
    } else {
        printf("  = %s  [%.4f ms]\n\n", res_str ? res_str : "0", timer_elapsed_ms(&timer));
    }

    /* Step 16: Store ans — symtab_set("ans", &result) */
    symtab_set(&repl->vars, "ans", &result);
    if (assign_var) {
        symtab_set(&repl->vars, assign_var, &result);
    }
    free(res_str);

    /* Step 17: Cleanup */
    value_free(&result);
    parsed_expression_free(&pe);
    token_list_free(&tokens);
    free(assign_var);
    free(line_copy);
    return 1;
}

/* -------------------------------------------------------------------------
 * Main Interactive REPL Entry Point
 * ------------------------------------------------------------------------- */
int run_repl(void) {
    ReplState repl;
    repl_state_init(&repl);

    printf("=================================================================\n");
    printf(" Arbitrary-Precision Calculator (ISO C11 REPL Engine)\n");
    printf(" Exact BigInt, BigDecimal, Complex Numbers & Taylor-Series Math\n");
    printf(" Type 'help' for commands, 'exit' or Ctrl+D to quit.\n");
    printf("=================================================================\n\n");

    while (repl.running) {
        /* Step 1: display_menu() */
        display_menu();

        /* Step 4: read_input_line() */
        char *line = read_input_line(stdin);
        if (!line) {
            /* EOF / Ctrl+D */
            printf("\nExiting.\n");
            break;
        }

        /* Execute full 17-step cycle */
        execute_repl_cycle(&repl, line);
        free(line);
    }

    repl_state_free(&repl);
    return 0;
}
