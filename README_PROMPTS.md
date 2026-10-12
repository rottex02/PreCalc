# Implementation Prompts Guide (All Parts)
**Mini Project 2: Arbitrary-Precision Calculator (Full Expression Engine)**  
*COEP Technological University · Department of Computer Science and Engineering*

---

<a id="table-of-contents"></a>
## Table of Contents
1. [Overview & Execution Roadmap](#overview)
2. [Part 1: Worklet 2 — Number Storage & Basic Arithmetic Primitives](#part-1)
3. [Part 2: Worklet 1 — Memory Infrastructure, String Conversions & High-Res Timer](#part-2)
4. [Part 3: Worklet 1 — Lexer & Tokenizer with Column-Accurate Tracking](#part-3)
5. [Part 4: Worklet 1 — Recursive-Descent Parser & AST Construction](#part-4)
6. [Part 5: Worklet 3 — Fast Multiplication (Karatsuba) & Long Division](#part-5)
7. [Part 6: Worklet 3 — Arbitrary-Precision Decimals & Complex Numbers](#part-6)
8. [Part 7: Worklet 3 — Scientific Functions (Taylor Series & Newton-Raphson)](#part-7)
9. [Part 8: Worklet 3 — AST Evaluator, Type Promotion & Symbol Table](#part-8)
10. [Part 9: System Integration — 17-Step Interactive REPL Loop](#part-9)
11. [Part 10: Verification, Differential Testing & Memory Audit](#part-10)

---

<a id="overview"></a>
## Overview
### Execution Roadmap & Structure

The implementation is broken down into 10 modular parts across the 3 project worklets:
* **Parts 1:** Leaf-level numeric primitives (Worklet 2).
* **Parts 2–4:** Memory, I/O, lexer, and recursive-descent parser (Worklet 1).
* **Parts 5–8:** Advanced algorithms, decimals, complex numbers, series functions, and AST evaluator (Worklet 3).
* **Parts 9–10:** 17-step REPL integration and comprehensive testing suite.

---

<a id="part-1"></a>
## Part 1
### Worklet 2: Number Storage & Basic Arithmetic Primitives

### Role & Objective
You are implementing Part 1 (Worklet 2: Number Storage & Basic Arithmetic) for an Arbitrary-Precision Scientific Calculator in ISO C (C99/C11) from first principles.

### Representation & Invariants
- Base: B = 10^9 (1,000,000,000) using 32-bit unsigned integers:
  #define BASE 1000000000U
  #define BASE_DIGITS 9
- Limb type: uint32_t. Accumulators for operations must use uint64_t.
- Limb Ordering: Least-significant limb first (digits[0] has weight (10^9)^0).
- Sign-and-Magnitude:
  typedef struct {
      int        sign;     /* +1, -1, or 0 */
      size_t     size;     /* Active limbs in use */
      size_t     capacity; /* Allocated limbs */
      uint32_t  *digits;   /* Dynamic base-10^9 limb array */
  } BigInteger;
- Canonical Zero Invariant: Normalized zero must be size = 1, digits[0] = 0, sign = 0. No leading zero limbs when size > 1.

### Functions to Implement
1. Normalization & Utilities:
   - void bigint_normalize(BigInteger *a);
   - int  bigint_is_zero(const BigInteger *a);
   - int  bigint_is_one(const BigInteger *a);
   - int  bigint_copy(BigInteger *dest, const BigInteger *src);
   - void bigint_swap(BigInteger *a, BigInteger *b);
   - int  bigint_resize(BigInteger *a, size_t new_size);
2. Comparisons:
   - int  bigint_compare_abs(const BigInteger *a, const BigInteger *b); (O(1) on size difference, O(n) on limbs)
   - int  bigint_compare(const BigInteger *a, const BigInteger *b);
   - Relational wrappers: bigint_equal, bigint_less, bigint_greater, bigint_less_equal, bigint_greater_equal.
3. Addition & Subtraction:
   - int  bigint_add_abs(BigInteger *res, const BigInteger *a, const BigInteger *b); (64-bit column accumulation sum % BASE, carry sum / BASE)
   - int  bigint_sub_abs(BigInteger *res, const BigInteger *a, const BigInteger *b); (borrow propagation, assumes |a| >= |b|)
   - int  bigint_add(BigInteger *res, const BigInteger *a, const BigInteger *b); (signed routing)
   - int  bigint_sub(BigInteger *res, const BigInteger *a, const BigInteger *b); (signed routing)
   - int  bigint_add_small(BigInteger *res, const BigInteger *a, int64_t val);
4. Multiplication:
   - int  bigint_mul_small(BigInteger *res, const BigInteger *a, uint32_t val);
   - int  bigint_mul_schoolbook(BigInteger *res, const BigInteger *a, const BigInteger *b); (O(n*m), 64-bit limb accumulation, normalize)

### Safety Contracts
- Handle destination aliasing safely (e.g. res == a or res == b).
- Return 1 on success, 0 on failure. Clean up all temporary structures without leaks.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-2"></a>
## Part 2
### Worklet 1: Memory Infrastructure, String Conversions & High-Res Timer

### Role & Objective
You are implementing Part 2 (Worklet 1: Memory Infrastructure, Lossless String Conversions & High-Res Timer) for the Arbitrary-Precision Calculator in ISO C.


### Structures
```c
typedef struct {
    struct timespec start, end;
    double elapsed_ms;
} Timer;
```

### Functions to Implement
1. BigInteger Dynamic Allocation Lifecycle:
   - void bigint_init(BigInteger *a); (Initializes fields to safe null/zero state)
   - int  bigint_init_capacity(BigInteger *a, size_t cap);
   - int  bigint_reserve(BigInteger *a, size_t min_cap); (Geometric capacity doubling)
   - void bigint_clear(BigInteger *a);
   - void bigint_free(BigInteger *a); (Frees heap digits and resets pointers)
2. Lossless String <-> BigInteger Conversions:
   - int  bigint_from_string(BigInteger *a, const char *str);
     * Parses optional leading '+' or '-'.
     * Groups decimal digits from right to left in chunks of 9 into uint32_t limbs.
     * Sets sign and calls bigint_normalize().
   - char *bigint_to_string(const BigInteger *a);
     * Allocates heap buffer.
     * Prints '-' if negative.
     * Prints MSB limb unpadded (%u) and all lower limbs zero-padded to 9 digits (%09u).
     * Returns heap pointer owned by caller.
3. High-Resolution Timer:
   - void   timer_start(Timer *t);
   - void   timer_stop(Timer *t);
   - double timer_elapsed_ms(const Timer *t); (Uses clock_gettime(CLOCK_MONOTONIC))

### Verification Tests
- Test round-trip conversion string -> BigInteger -> string on 1-digit, 9-digit, 18-digit, and 200-digit numbers.
- Verify capacity geometric resizing and verify zero memory leaks under AddressSanitizer.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-3"></a>
## Part 3
### Worklet 1: Lexer & Tokenizer with Column-Accurate Tracking

### Role & Objective
You are implementing Part 3 (Worklet 1: Lexer & Tokenizer with Column-Accurate Tracking) for the Arbitrary-Precision Calculator in ISO C.

### Structures
```c
typedef enum {
    TK_NUMBER,   /* e.g. "123", "45.67", "1e-5" */
    TK_IDENT,    /* e.g. "sin", "gcd", "x", "ans" */
    TK_OP,       /* e.g. "+", "-", "*", "/", "%", "^", "!" */
    TK_LPAREN,   /* "(" */
    TK_RPAREN,   /* ")" */
    TK_COMMA,    /* "," */
    TK_END       /* End-of-input sentinel */
} TokenType;

typedef struct {
    TokenType type;
    char     *text;
    size_t    pos; /* 1-indexed column for visual caret error reporting */
} Token;

typedef struct {
    Token  *items;
    size_t  count;
    size_t  capacity;
} TokenList;

typedef struct {
    int    code;
    size_t position;
    char   message[256];
} ParseError;
```

### Functions to Implement
1. Line Input & Pre-validation:
   - char *read_input_line(FILE *stream); (Dynamically reads unbounded input line)
   - void  trim_whitespace(char *str);
   - int   validate_expression(const char *input, size_t *err_pos); (Checks allowed characters & parenthesis balance before tokenization)
2. Tokenizer (Lexer):
   - void  token_list_init(TokenList *tl);
   - void  token_list_free(TokenList *tl);
   - int   lex_tokenize(const char *input, TokenList *out, ParseError *err);
     * Scans left-to-right tracking column index pos.
     * Extracts numeric literals (digits, optional '.', optional exponent 'e'/'E').
     * Extracts identifiers ([a-zA-Z_][a-zA-Z0-9_]*).
     * Extracts operators and punctuation.
     * Appends TK_END at the end.
     * On illegal character: records exact column position in err->position.

### Verification Tests
- Test tokenizing expressions like: "1 + 2*3^2 - (40/8) + gcd(48, 18)".
- Verify that every token's pos correctly points to its column in the original string.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-4"></a>
## Part 4
### Worklet 1: Recursive-Descent Parser & AST Construction

### Role & Objective
You are implementing Part 4 (Worklet 1: Recursive-Descent Parser & AST Construction) for the Arbitrary-Precision Calculator in ISO C.


### Abstract Syntax Tree Representation
```c
typedef enum {
    NODE_NUMBER, NODE_VAR, NODE_UNARY, NODE_BINARY, NODE_CALL
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_NEG, OP_POS, OP_FACT
} OpCode;

typedef struct ExprNode {
    NodeType type;
    size_t   pos;
    union {
        Value   number;
        char   *var_name;
        struct { OpCode op; struct ExprNode *left, *right; } bin;
        struct { OpCode op; struct ExprNode *operand;      } un;
        struct { int fn_id; size_t argc; struct ExprNode **args; } call;
    } as;
} ExprNode;

typedef struct {
    char       *source;
    TokenList   tokens;
    ExprNode   *root;
    ParseError  error;
} ParsedExpression;
```

### Grammar to Implement (EBNF)
- expression -> term      (( '+' | '-' ) term)*
- term       -> power     (( '*' | '/' | '%' ) power)*
- power      -> unary     ( '^' power )?               [Right-associative]
- unary      -> ( '+' | '-' ) unary | postfix          [Positional sign disambiguation]
- postfix    -> primary   ( '!' )*                     [Factorial]
- primary    -> NUMBER | IDENT | IDENT '(' arglist ')' | '(' expression ')'
- arglist    -> expression ( ',' expression )*

### Functions to Implement
1. Node Constructors & Destructors:
   - ExprNode *node_new_number(const char *str, size_t pos);
   - ExprNode *node_new_var(const char *name, size_t pos);
   - ExprNode *node_new_binary(OpCode op, ExprNode *left, ExprNode *right, size_t pos);
   - ExprNode *node_new_unary(OpCode op, ExprNode *operand, size_t pos);
   - ExprNode *node_new_call(int fn_id, ExprNode **args, size_t argc, size_t pos);
   - void      free_expr_node(ExprNode *root); (Single post-order recursive pass freeing all child nodes and argument vectors)
2. Parser Engine:
   - int parse_expression_tree(const TokenList *t, ParsedExpression *out);
   - Validate function argument arity against the FUNCTIONS[] table at parse time.
   - Positional Unary Sign Disambiguation: A '+' or '-' is unary if parsed in parse_unary without preceding binary operand.

### Verification Tests
- Test AST construction for "a * b + (c * d - sin(e))" and verify tree structure.
- Test unary sign precedence in "-3 * -4 - (-5)".
- Verify free_expr_node reclaims all allocations under Valgrind.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-5"></a>
## Part 5
### Worklet 3: Fast Multiplication (Karatsuba) & Long Division

### Role & Objective
You are implementing Part 5 (Worklet 3: Fast Multiplication & Multi-Limb Long Division) for the Arbitrary-Precision Calculator in ISO C.


### Structures
```c
typedef struct {
    BigInteger quotient;
    BigInteger remainder;
} DivisionResult;
```

### Functions to Implement
1. Karatsuba Multiplication:
   - int bigint_mul_karatsuba(BigInteger *res, const BigInteger *a, const BigInteger *b);
     * Base case: when size < KARATSUBA_CROSSOVER (tuned around 16-32 limbs), delegate to bigint_mul_schoolbook.
     * Split at k = ceil(n/2): a = a1*B^k + a0, b = b1*B^k + b0.
     * Compute z2 = a1*b1, z0 = a0*b0, z1 = (a1+a0)*(b1+b0) - z2 - z0.
     * Recombine: z2*B^(2k) + z1*B^k + z0.
   - int bigint_mul(BigInteger *res, const BigInteger *a, const BigInteger *b); (Selector choosing Schoolbook vs Karatsuba)
2. Multi-Limb Long Division:
   - int bigint_divide(DivisionResult *res, const BigInteger *dividend, const BigInteger *divisor);
     * Knuth Algorithm D / trial digit long division.
     * Normalize divisor so MSB limb >= BASE/2.
     * Form trial quotient limbs q_hat, correct if remainder goes negative.
     * Simultaneously compute exact quotient and remainder.
     * Stated Bound: 100-digit / 200-digit division MUST execute in << 1.0 second.
   - int bigint_mod(BigInteger *res, const BigInteger *a, const BigInteger *b);
3. Number Theory Primitives:
   - int bigint_gcd(BigInteger *res, const BigInteger *a, const BigInteger *b); (Euclidean algorithm by repeated remainder)
   - int bigint_pow(BigInteger *res, const BigInteger *base, uint64_t exp); (Binary exponentiation O(log exp))
   - int bigint_mod_pow(BigInteger *res, const BigInteger *base, const BigInteger *exp, const BigInteger *mod);

### Verification Tests
- Benchmark 100-digit by 200-digit division and report execution time (assert < 1.0 s).
- Verify Karatsuba results match schoolbook multiplication across large inputs.
- Verify gcd(48, 18) == 6 and large coprime pairs.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-6"></a>
## Part 6
### Worklet 3: Arbitrary-Precision Decimals & Complex Numbers

### Role & Objective
You are implementing Part 6 (Worklet 3: Arbitrary-Precision Decimals & Complex Numbers) for the Arbitrary-Precision Calculator in ISO C.

### Structures
```c
typedef enum { ROUND_HALF_UP, ROUND_DOWN, ROUND_TRUNC } RoundingMode;

typedef struct {
    BigInteger significand;
    int        scale; /* 123.456 -> significand 123456, scale 3 */
} BigDecimal;

typedef struct {
    BigDecimal real;
    BigDecimal imag;
} Complex;
```

### Functions to Implement
1. BigDecimal Primitives:
   - void decimal_init(BigDecimal *d);
   - void decimal_free(BigDecimal *d);
   - int  decimal_from_string(BigDecimal *d, const char *str);
   - char *decimal_to_string(const BigDecimal *d);
   - int  decimal_add(BigDecimal *res, const BigDecimal *a, const BigDecimal *b); (Scale alignment by 10^(delta scale))
   - int  decimal_sub(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);
   - int  decimal_mul(BigDecimal *res, const BigDecimal *a, const BigDecimal *b); (Significand multiply, add scales)
   - int  decimal_div(BigDecimal *res, const BigDecimal *a, const BigDecimal *b, int precision); (Scale numerator by 10^precision before division)
   - int  decimal_round(BigDecimal *d, int target_scale, RoundingMode mode);
2. Complex Number Primitives:
   - void complex_init(Complex *c);
   - void complex_free(Complex *c);
   - int  complex_add(Complex *res, const Complex *a, const Complex *b);
   - int  complex_sub(Complex *res, const Complex *a, const Complex *b);
   - int  complex_mul(Complex *res, const Complex *a, const Complex *b); ((ac - bd) + (ad + bc)i)
   - int  complex_div(Complex *res, const Complex *a, const Complex *b, int precision); (Multiplication by conjugate)
   - char *complex_to_string(const Complex *c);

### Verification Tests
- Test decimal addition: 12.345 + 0.000678 = 12.345678.
- Test decimal division: 1 / 3 to 50 decimal places.
- Test complex multiplication: (1 + 2i) * (3 + 4i) = -5 + 10i.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-7"></a>
## Part 7
### Worklet 3: Scientific Functions (Taylor Series & Newton-Raphson)

### Role & Objective
You are implementing Part 7 (Worklet 3: Scientific Functions via Taylor Series & Newton-Raphson) for the Arbitrary-Precision Calculator in ISO C.


### Mathematical Functions Specification
Evaluate each function to the target precision configured in EvalContext:
1. sin(x) and cos(x):
   - Range reduction: reduce x modulo 2*pi.
   - Truncated Maclaurin series:
     sin(x) = sum_{k=0}^N (-1)^k * x^(2k+1) / (2k+1)!
     cos(x) = sum_{k=0}^N (-1)^k * x^(2k) / (2k)!
   - Term cutoff when |term| < 10^(-precision).
2. exp(x):
   - Split x = m + f (m integer, 0 <= f < 1).
   - Compute e^m * sum_{k=0}^N f^k / k!.
3. ln(x):
   - Range reduction using ln(x) = ln(y * 10^k) = k*ln(10) + 2*sum_{k=0}^N (1/(2k+1)) * ((y-1)/(y+1))^(2k+1).
4. sqrt(x):
   - Arbitrary-precision Newton-Raphson iteration: y_{k+1} = 0.5 * (y_k + x / y_k).
   - Terminate when |y_{k+1} - y_k| < 10^(-precision).

### Function Dispatch Interface
```c
typedef struct {
    const char *name;
    int         min_args, max_args;
    int (*impl)(Value *out, const Value *args, size_t argc, const EvalContext *ctx);
} FunctionEntry;

extern const FunctionEntry FUNCTIONS[];
const FunctionEntry *func_lookup(const char *name);
```

### Verification Tests
- Verify sin(pi/6) evaluates to 0.50000000000000000000 to 20 decimal places.
- Verify exp(1) evaluates to e (2.718281828459045...).
- Verify sqrt(2) to 50 digits matches known mathematical constant.

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-8"></a>
## Part 8
### Worklet 3: AST Evaluator, Type Promotion & Symbol Table

### Role & Objective
You are implementing Part 8 (Worklet 3: AST Evaluator, Unified Value System & Symbol Table) for the Arbitrary-Precision Calculator in ISO C.

### Structures
```c
typedef enum { VAL_INT, VAL_DEC, VAL_COMPLEX } ValueType;

typedef struct {
    ValueType type;
    union {
        BigInteger i;
        BigDecimal d;
        Complex    c;
    } as;
} Value;

typedef struct {
    char  *name;
    Value  value;
} Symbol;

typedef struct {
    Symbol *symbols;
    size_t  count;
    size_t  capacity;
} SymbolTable;

typedef struct {
    int          precision;
    AngleMode    angle_mode; /* RADIAN or DEGREE */
    RoundingMode rounding;
    SymbolTable *vars;
    int          depth;      /* Recursion depth guard */
    EvalError    error;
} EvalContext;
```

### Functions to Implement
1. Value Type System:
   - void value_init(Value *v, ValueType type);
   - void value_free(Value *v);
   - int  value_copy(Value *dest, const Value *src);
   - int  value_promote(Value *a, Value *b); (Lifts narrower operand: VAL_INT -> VAL_DEC -> VAL_COMPLEX)
   - char *value_to_string(const Value *v);
2. Symbol Table:
   - void symtab_init(SymbolTable *st);
   - void symtab_free(SymbolTable *st);
   - int  symtab_set(SymbolTable *st, const char *name, const Value *v);
   - int  symtab_get(const SymbolTable *st, const char *name, Value *out);
   - Preload constants: "pi", "e", and previous result "ans".
3. AST Evaluator:
   - int  eval_node(const ExprNode *n, EvalContext *ctx, Value *out);
     * Post-order walk: evaluate children first, then parent.
     * Guard against infinite recursion: if (ctx->depth > MAX_DEPTH) error.
   - int  eval_binary(OpCode op, const Value *left, const Value *right, EvalContext *ctx, Value *out);
   - int  eval_unary(OpCode op, const Value *operand, EvalContext *ctx, Value *out);
   - int  eval_call(int fn_id, const Value *args, size_t argc, const EvalContext *ctx, Value *out);

### Verification Tests
- Test full expression evaluation: "1 + 2*3^2 - (40/8) + gcd(48, 18)" == 20.
- Test mixed operand promotion: "10 + 2.5" == 12.5.
- Test variable storage and retrieval ("x = 10; x * 2").

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-9"></a>
## Part 9
### System Integration: 17-Step Interactive REPL Loop

### Role & Objective
You are implementing Part 9 (System Integration & 17-Step REPL Control Loop) uniting Worklet 1, Worklet 2, and Worklet 3 into the final application executable.


### The 17-Step Execution Cycle (per line)
Implement the exact sequence from the design phase review:
1.  display_menu() — Prompt user (e.g. "> ").
2.  get_operation_choice() — Check for REPL commands: "exit", "quit", "help", "mode", "precision <n>".
3.  Exit check — If exit requested, break loop.
4.  read_input_line() — Read unbounded text line from stdin.
5.  validate_expression() — Check character validity and balanced parentheses.
6.  lex_tokenize(&tokens) — Tokenize into TokenList.
7.  parse_expression(&tokens) — Build ExprNode AST root.
8.  Reject on parse error — On failure, print original line, print caret '^' under error column, print message, free tokens, continue loop.
9.  eval_context_init(&ctx) — Setup context, symbols, precision.
10. timer_start(&timer) — START TIMED WINDOW (brackets step 11 only).
11. eval_node(root, &ctx) — Post-order AST evaluation.
12. timer_stop(&timer) — STOP TIMED WINDOW.
13. Handle eval failure — If evaluation failed (e.g. division by zero), print error, cleanup, continue loop.
14. value_to_string(&result) — Format exact result digits.
15. Display — Print exact digits and timer_elapsed_ms(&timer).
16. Store ans — symtab_set(ctx.vars, "ans", &result), free printable string.
17. Cleanup — free_expr_node(root), token_list_free(&tokens), value_free(&result).

### User Experience Details
- On syntax errors, render the error position with a caret indicator:
  > 2 * (3 + sin(4)
           ^
  ParseError [ERR_UNBALANCED_PAREN at column 5]: unclosed '('

[▲ Back to Table of Contents](#table-of-contents)

---

<a id="part-10"></a>
## Part 10
### Verification, Differential Testing & Memory Audit

### Role & Objective
You are implementing Part 10 (Verification, Differential Testing & Memory Audit) to guarantee mathematical correctness, time bounds, and zero memory leaks.

### Tasks to Implement
1. Test Runner Entry Point:
   - int run_all_tests(void); — Invoked via command line flag (e.g. ./calculator --test).
2. Worklet Test Suites:
   - test_worklet1_lexer_parser(): Verifies token positions, AST topologies, and precedence.
   - test_worklet2_storage_arithmetic(): Verifies limb boundaries, carry/borrow propagation, schoolbook multiplication.
   - test_worklet3_advanced_functions(): Verifies Karatsuba, long division, decimals, complex numbers, Taylor series.
3. Differential Testing Suite:
   - Compare outputs of 100+ expressions against GNU bc and Python reference values:
     * "1 + 2*3^2 - (40/8) + gcd(48, 18)" -> 20
     * "2^100"
     * "sin(0.5)"
4. Stated Performance Bound:
   - benchmark_division_time(): Generate a 100-digit dividend and 200-digit divisor; assert elapsed execution time < 1.0 second.
5. Memory Leak Audit:
   - Build with AddressSanitizer and UndefinedBehaviorSanitizer:
     gcc -Wall -Wextra -pedantic -std=c11 -g -fsanitize=address,undefined big_integer_calculator.c -o calculator_debug
   - Execute under Valgrind:
     valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./calculator --test
   - Assert zero leaked bytes and zero aliasing faults.

[▲ Back to Table of Contents](#table-of-contents)


