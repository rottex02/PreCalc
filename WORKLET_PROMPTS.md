# Worklet Implementation Prompts
**Mini Project 2: Arbitrary-Precision Calculator (Full Expression Engine)**

These prompts are tailored for assigning each worklet to an AI coding assistant, subagent, or team member. Each prompt contains the exact responsibilities, frozen interface contracts, memory invariants, algorithmic constraints, and verification requirements from the project specification.

---

## 📌 Dependency & Execution Order
1. **Worklet 2 (Numeric Core)** can be implemented immediately (implements fundamental `BigInteger` limb storage, comparisons, addition, subtraction, schoolbook multiplication).
2. **Worklet 1 (Infrastructure & Parser)** implements the memory lifecycle framework, string $\leftrightarrow$ `BigInteger` conversion, lexer, recursive-descent parser, and AST node constructors/destructors.
3. **Worklet 3 (Evaluator & Advanced Math)** layers on top of both Worklets 1 and 2, implementing Karatsuba, long division, decimals, complex numbers, Taylor series, and the AST interpreter.
4. **Integration Phase** binds all three into the 17-step REPL loop, testing suite, and timer profiling.

---

# Prompt for Worklet 2: Number Storage & Basic Arithmetic (Numeric Core)

```markdown
### Role & Objective
You are implementing **Worklet 2 (Number Storage & Basic Arithmetic)** for an arbitrary-precision scientific calculator in standard ISO C (C99/C11) from first principles (no external bignum libraries). Your code provides the fundamental numeric engine and leaf operations called by every expression node.

### Core Data Structure & Representation
* Base: $B = 10^9$ (`uint32_t digits[]`), with `BASE = 1000000000U` and `BASE_DIGITS = 9`.
* Digit Ordering: Least-significant limb first (`digits[0]` carries weight $(10^9)^0$).
* Sign-and-Magnitude: `sign` $\in \{+1, -1, 0\}$; `digits[]` stores the unsigned magnitude.
* Normalization Invariant: No leading zero limbs. Zero is uniquely represented as `size = 1`, `digits[0] = 0`, `sign = 0`.

```c
typedef struct {
    int        sign;     /* +1, -1, or 0 */
    size_t     size;     /* Active limbs in use */
    size_t     capacity; /* Allocated limbs */
    uint32_t  *digits;   /* Dynamic base-10^9 array */
} BigInteger;
```

### Required Function Implementations
Implement the following exact function signatures:

1. **Normalization & Lifecycle Utilities:**
   - `void bigint_normalize(BigInteger *a);` — Strips high zero limbs; enforces canonical zero.
   - `int bigint_is_zero(const BigInteger *a);`
   - `int bigint_is_one(const BigInteger *a);`
   - `int bigint_copy(BigInteger *dest, const BigInteger *src);`
   - `void bigint_swap(BigInteger *a, BigInteger *b);`
   - `int bigint_resize(BigInteger *a, size_t new_size);`

2. **Comparisons:**
   - `int bigint_compare_abs(const BigInteger *a, const BigInteger *b);` — Returns -1 if $|a| < |b|$, 0 if $|a| == |b|$, +1 if $|a| > |b|$. Must be $O(1)$ when sizes differ.
   - `int bigint_compare(const BigInteger *a, const BigInteger *b);` — Signed three-way comparison.
   - Relational wrappers: `bigint_equal`, `bigint_less`, `bigint_greater`, `bigint_less_equal`, `bigint_greater_equal`.

3. **Magnitude Addition & Subtraction:**
   - `int bigint_add_abs(BigInteger *res, const BigInteger *a, const BigInteger *b);` — 64-bit column accumulation (`uint64_t`), carry propagation into `res`.
   - `int bigint_sub_abs(BigInteger *res, const BigInteger *a, const BigInteger *b);` — Assumes $|a| \ge |b|$; column subtraction with borrow propagation.

4. **Signed Addition & Subtraction:**
   - `int bigint_add(BigInteger *res, const BigInteger *a, const BigInteger *b);` — Signed magnitude routing based on operand signs.
   - `int bigint_sub(BigInteger *res, const BigInteger *a, const BigInteger *b);` — Signed magnitude routing.
   - `int bigint_add_small(BigInteger *res, const BigInteger *a, int64_t val);`

5. **Schoolbook Multiplication:**
   - `int bigint_mul_small(BigInteger *res, const BigInteger *a, uint32_t val);`
   - `int bigint_mul_schoolbook(BigInteger *res, const BigInteger *a, const BigInteger *b);` — $O(n \cdot m)$ algorithm. Accumulate $a_i \times b_j$ into 64-bit limb cells; propagate carries; normalize.

### Strict Contracts & Invariants
* **No Memory Aliasing:** Operations must safely support `res` pointing to an uninitialized struct or distinct pre-reserved struct. If `res == a` or `res == b`, handle aliasing cleanly via temporary storage if necessary.
* **Return Conventions:** Functions returning `int` must return `1` on success and `0` on memory allocation failure.
* **Pure C:** Use only standard library headers (`<stdint.h>`, `<stdlib.h>`, `<string.h>`). No compiler extensions.

### Verification Deliverables
Write standalone unit tests validating:
* Normalization on large sequences of zeros and boundary values.
* Carry propagation across 10+ limb boundaries.
* Subtraction underflow/borrow across multiple zero limbs (e.g. $10^{18} - 1$).
* Schoolbook multiplication results matching known 50-digit products.
```

---

# Prompt for Worklet 1: Input, Lexing, Parsing, I/O & Infrastructure

```markdown
### Role & Objective
You are implementing **Worklet 1 (Input, Lexing, Parsing, I/O & Infrastructure)** for an arbitrary-precision scientific calculator in standard ISO C. You are responsible for the shared memory lifecycle, text $\leftrightarrow$ numeric boundary, tokenization, recursive-descent AST parser with positional sign disambiguation, operator/function tables, and profiling timers.

### Core Data Structures
```c
/* Tokenizer */
typedef enum {
    TK_NUMBER, TK_IDENT, TK_OP,
    TK_LPAREN, TK_RPAREN, TK_COMMA, TK_END
} TokenType;

typedef struct {
    TokenType type;
    char     *text;
    size_t    pos; /* 1-indexed column for caret error reporting */
} Token;

typedef struct {
    Token  *items;
    size_t  count;
    size_t  capacity;
} TokenList;

/* AST Node Types & Structure */
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

typedef struct {
    struct timespec start, end;
    double elapsed_ms;
} Timer;
```

### Required Function Implementations
Implement the following exact function signatures:

1. **BigInteger Memory Skeleton:**
   - `void bigint_init(BigInteger *a);`
   - `int bigint_init_capacity(BigInteger *a, size_t cap);`
   - `int bigint_reserve(BigInteger *a, size_t min_cap);`
   - `void bigint_free(BigInteger *a);`
   - `void bigint_clear(BigInteger *a);`

2. **Input & Validation:**
   - `char *read_input_line(FILE *stream);` — Reads arbitrary-length line dynamically.
   - `void trim_whitespace(char *str);`
   - `int validate_expression(const char *input, size_t *err_pos);` — Verifies character set and balanced parentheses before lexing.

3. **Lexer (Tokenizer):**
   - `int lex_tokenize(const char *input, TokenList *out, ParseError *err);`
   - `void token_list_init(TokenList *tl);`
   - `void token_list_free(TokenList *tl);`
   - Lexes numeric literals (including decimals and exponents), identifiers, parentheses, commas, operators.

4. **Recursive-Descent Parser:**
   - Implement structural grammar:
     * `parse_expression` $\rightarrow$ term $((+ \mid -) \text{ term})^*$
     * `parse_term` $\rightarrow$ power $((* \mid / \mid \%) \text{ power})^*$
     * `parse_power` $\rightarrow$ unary $(^{\wedge} \text{ power})^?$ (Right-associative)
     * `parse_unary` $\rightarrow (+ \mid -) \text{ unary} \mid \text{postfix}$ (Disambiguates sign based on grammar position)
     * `parse_postfix` $\rightarrow$ primary $(!)^*$
     * `parse_primary` $\rightarrow \text{NUMBER} \mid \text{IDENT} \mid \text{IDENT}(\text{arglist}) \mid (\text{expression})$
   - `int parse_expression_tree(const TokenList *t, ParsedExpression *out);`
   - Check function arity against `FUNCTIONS[]` table at parse time.

5. **AST Lifecycle:**
   - Constructors: `node_new_number()`, `node_new_var()`, `node_new_binary()`, `node_new_unary()`, `node_new_call()`.
   - Destructor: `void free_expr_node(ExprNode *root);` — Recursively frees all nodes, argument vectors, and strings.

6. **Lossless String $\leftrightarrow$ BigInteger Conversions:**
   - `int bigint_from_string(BigInteger *a, const char *str);` — Packs 9 decimal digits per limb, least-significant first.
   - `char *bigint_to_string(const BigInteger *a);` — Caller frees; MSB unpadded, inner limbs formatted with `%09u`.

7. **High-Resolution Timer:**
   - `void timer_start(Timer *t);`
   - `void timer_stop(Timer *t);`
   - `double timer_elapsed_ms(const Timer *t);` — Uses `clock_gettime(CLOCK_MONOTONIC)`.

### Strict Invariants & Error Handling
* Exact Caret Error Reporting: `ParseError{code, position, message}` must record the column index so the REPL prints `^` beneath the exact offending token.
* Single AST ownership: No node shared between multiple parents; `free_expr_node` must completely reclaim heap memory without leaks.

### Verification Deliverables
* Unit test verifying correct AST shape for `a * b + (c * d - sin(e))`.
* Unit test validating unary sign disambiguation for `-3 * -4 - (-5)`.
* Round-trip conversion test: `string -> BigInteger -> string` for 100+ digit numbers.
* Zero memory leaks under Valgrind/ASan.
```

---

# Prompt for Worklet 3: Expression Evaluation, Advanced Arithmetic & Functions

```markdown
### Role & Objective
You are implementing **Worklet 3 (Evaluation, Advanced Arithmetic & Functions)** for an arbitrary-precision scientific calculator in standard ISO C. You are responsible for the AST interpreter, the unified `Value` system with type promotion, Karatsuba fast multiplication, multi-limb long division, arbitrary-precision decimals, complex numbers, and truncated Taylor series.

### Core Data Structures
```c
typedef enum { VAL_INT, VAL_DEC, VAL_COMPLEX } ValueType;

typedef struct {
    BigInteger significand;
    int        scale; /* 123.456 -> significand 123456, scale 3 */
} BigDecimal;

typedef struct {
    BigInteger quotient;
    BigInteger remainder;
} DivisionResult;

typedef struct {
    BigDecimal real;
    BigDecimal imag;
} Complex;

typedef struct {
    ValueType type;
    union {
        BigInteger i;
        BigDecimal d;
        Complex    c;
    } as;
} Value;

typedef struct {
    int          precision;   /* Fractional digits for decimals */
    AngleMode    angle_mode;  /* RADIAN or DEGREE */
    RoundingMode rounding;
    SymbolTable *vars;        /* Variables, pi, e, ans */
    int          depth;       /* Recursion depth guard */
    EvalError    error;
} EvalContext;

typedef struct {
    const char *name;
    int         min_args, max_args;
    int (*impl)(Value *out, const Value *args, size_t argc, const EvalContext *ctx);
} FunctionEntry;
```

### Required Function Implementations
Implement the following exact function signatures:

1. **Value Type System & Promotion:**
   - `void value_init(Value *v, ValueType type);`
   - `void value_free(Value *v);`
   - `int value_copy(Value *dest, const Value *src);`
   - `int value_promote(Value *a, Value *b);` — Promotes narrower operand: $\text{VAL\_INT} \rightarrow \text{VAL\_DEC} \rightarrow \text{VAL\_COMPLEX}$.
   - `char *value_to_string(const Value *v);` — Caller frees.

2. **Fast Multiplication & Selector:**
   - `int bigint_mul_karatsuba(BigInteger *res, const BigInteger *a, const BigInteger *b);`
     * Split at $k = \lceil n/2 \rceil$: $z_2 = a_1 b_1$, $z_0 = a_0 b_0$, $z_1 = (a_1 + a_0)(b_1 + b_0) - z_2 - z_0$.
     * Combine: $z_2 B^{2k} + z_1 B^k + z_0$.
   - `int bigint_mul(BigInteger *res, const BigInteger *a, const BigInteger *b);` — Selector: automatically chooses schoolbook below crossover threshold and Karatsuba above it.

3. **Multi-Limb Long Division & Number Theory:**
   - `int bigint_divide(DivisionResult *res, const BigInteger *dividend, const BigInteger *divisor);`
     * Must meet stated project bound: $100\text{-digit} \div 200\text{-digit}$ division executes in $\ll 1.0\text{ s}$.
     * Simultaneously computes exact quotient and remainder. Returns failure if divisor is zero.
   - `int bigint_mod(BigInteger *res, const BigInteger *a, const BigInteger *b);`
   - `int bigint_gcd(BigInteger *res, const BigInteger *a, const BigInteger *b);` — Euclidean algorithm.
   - `int bigint_pow(BigInteger *res, const BigInteger *base, uint64_t exp);` — Binary exponentiation.
   - `int bigint_mod_pow(BigInteger *res, const BigInteger *base, const BigInteger *exp, const BigInteger *mod);`

4. **Arbitrary-Precision Decimals (`BigDecimal`):**
   - `int decimal_from_string(BigDecimal *d, const char *str);`
   - `int decimal_add(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);` — Align scales before adding.
   - `int decimal_sub(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);`
   - `int decimal_mul(BigDecimal *res, const BigDecimal *a, const BigDecimal *b);` — Add scales.
   - `int decimal_div(BigDecimal *res, const BigDecimal *a, const BigDecimal *b, int precision);`
   - `int decimal_round(BigDecimal *d, int target_scale, RoundingMode mode);`

5. **Complex Numbers:**
   - `int complex_add(Complex *res, const Complex *a, const Complex *b);`
   - `int complex_sub(Complex *res, const Complex *a, const Complex *b);`
   - `int complex_mul(Complex *res, const Complex *a, const Complex *b);` — $(ac - bd) + (ad + bc)i$.
   - `int complex_div(Complex *res, const Complex *a, const Complex *b, int precision);` — Conjugate division.

6. **Mathematical Functions (Taylor Series & Iteration):**
   - Truncated series computed up to `ctx->precision`:
     * `int fn_sin(Value *out, const Value *args, size_t argc, const EvalContext *ctx);`
     * `int fn_cos(Value *out, const Value *args, size_t argc, const EvalContext *ctx);`
     * `int fn_exp(Value *out, const Value *args, size_t argc, const EvalContext *ctx);`
     * `int fn_ln(Value *out, const Value *args, size_t argc, const EvalContext *ctx);`
     * `int fn_sqrt(Value *out, const Value *args, size_t argc, const EvalContext *ctx);` — Newton-Raphson.

7. **AST Evaluator & Symbol Table:**
   - `int eval_node(const ExprNode *n, EvalContext *ctx, Value *out);`
     * Post-order traversal: children evaluated before parent node.
     * Guard recursion depth against stack overflow (`ctx->depth > MAX_DEPTH`).
   - `int eval_binary(OpCode op, const Value *left, const Value *right, EvalContext *ctx, Value *out);`
   - `int eval_unary(OpCode op, const Value *operand, EvalContext *ctx, Value *out);`
   - `int eval_call(int fn_id, const Value *args, size_t argc, const EvalContext *ctx, Value *out);`
   - `int symtab_get(SymbolTable *st, const char *name, Value *out);`
   - `int symtab_set(SymbolTable *st, const char *name, const Value *val);`

### Strict Invariants & Performance
* Division Time Bound: Must execute a 100-digit by 200-digit division in under 1 second.
* Zero Divisor Handling: Reject division by zero gracefully and record `ERR_DIV_BY_ZERO` in `ctx->error`.
* No Memory Leaks: Free all temporary `BigInteger` and `BigDecimal` variables created during Taylor iterations.

### Verification Deliverables
* Benchmark showing division time for $100\text{-digit} \div 200\text{-digit}$ numbers.
* Karatsuba crossover curve verification vs schoolbook multiplication.
* Evaluation of composite expression: `1 + 2*3^2 - (40/8) + gcd(48, 18)` yielding exactly `20`.
* Verification of `sin(pi/6)` yielding `0.5000000000...` to configured decimal precision.
```

---

# Prompt for Integration, 17-Step REPL & Test Harness

```markdown
### Role & Objective
You are implementing the **System Integration, REPL Control Loop, and Comprehensive Test Harness** connecting Worklet 1, Worklet 2, and Worklet 3 into an executable application (`big_integer_calculator.c`).

### REPL Control Loop Specification
Implement the exact 17-step loop specified in the design phase review:
1. `display_menu()` — Show prompt (`> `).
2. `get_operation_choice()` / handle commands (`exit`, `help`, `mode`, `precision`).
3. Exit check (`choice == EXIT` $\rightarrow$ break).
4. `read_input_line()` — Read raw line.
5. `validate_expression()` — Verify characters and parenthesis balancing.
6. `lex_tokenize(&tokens)` — Scan into TokenList.
7. `parse_expression(&tokens)` — Build AST root.
8. Parse error check — If error, print input line, print caret `^` at `error.position`, display message, free tokens, and continue loop.
9. `eval_context_init(&ctx)` — Set precision, symbols, and reset depth.
10. `timer_start(&timer)` — **Start benchmark timer (brackets step 11 only).**
11. `eval_node(root, &ctx)` — Post-order walk over AST.
12. `timer_stop(&timer)` — **Stop benchmark timer.**
13. Evaluation error check — If error, display message, free AST/tokens, and continue loop.
14. `value_to_string(&result)` — Convert result to heap-allocated string.
15. Display output: print exact digits and `timer_elapsed_ms(&timer)`.
16. Store result in symbol table: `symtab_set("ans", &result)`, free printed string buffer.
17. Cleanup: `free_expr_node(root)`, `token_list_free(&tokens)`, `value_free(&result)`.

### Test Harness Specification
Implement `int run_all_tests(void);`:
1. **Worklet 2 Suite:** Limb normalization, carry/borrow propagation, magnitude comparison, schoolbook multiplication against known large products.
2. **Worklet 1 Suite:** Tokenizer position checks, AST topology checks, operator precedence and right-associativity, positional unary sign disambiguation.
3. **Worklet 3 Suite:** Karatsuba multiplication, division time bound benchmark ($100\text{-digit} \div 200\text{-digit} < 1\text{ s}$), decimal rounding, Taylor series convergence, variable chaining via `ans`.
4. **Differential Test Suite:** Compare results of 100+ random and edge-case expressions against reference outputs.
5. **Negative Test Suite:** Verify clean error handling (no segfaults, no leaks) on malformed inputs (`2 * (3 + sin(4)`, `1 / 0`, `gcd(1)`, `unknown_func()`).

### Compilation & Build Verification
* Clean build with:
  ```bash
  gcc -Wall -Wextra -pedantic -std=c11 -O2 big_integer_calculator.c -o calculator
  ```
* Leak verification with:
  ```bash
  valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./calculator --test
  ```
```

