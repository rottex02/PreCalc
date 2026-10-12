CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2 -Iworklet1 -Iworklet2 -Iworklet3
DEBUG_FLAGS = -Wall -Wextra -pedantic -std=c11 -g -fsanitize=address,undefined -Iworklet1 -Iworklet2 -Iworklet3

SRC_W2 = worklet2/bigint.c
SRC_W1_IO = worklet1/bigint_io.c worklet1/timer.c
SRC_W1_PARSE = worklet1/lexer.c worklet1/ast.c worklet1/parser.c
SRC_W3_MATH = worklet3/fast_math.c
SRC_W3_DEC = worklet3/decimal.c worklet3/complex.c
SRC_W3_FUNC = worklet3/functions.c
SRC_W3_EVAL = worklet3/eval.c
SRC_W1_REPL = worklet1/repl.c

SRC_W1_TEST10 = worklet1/test_part10.c

all: calculator test_worklet2 test_worklet1_part2 test_worklet1_part3 test_worklet1_part4 test_worklet3_part5 test_worklet3_part6 test_worklet3_part7 test_worklet3_part8 test_worklet1_part9 test_part10

calculator: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) $(SRC_W1_TEST10) main.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) $(SRC_W1_TEST10) main.c -lm -o calculator

test_part10: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) $(SRC_W1_TEST10)
	$(CC) $(DEBUG_FLAGS) -DMAIN_TEST_PART10 $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) $(SRC_W1_TEST10) -lm -o test_part10

test_worklet2: $(SRC_W2) worklet2/test_part1.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) worklet2/test_part1.c -o test_worklet2

test_worklet1_part2: $(SRC_W2) $(SRC_W1_IO) worklet1/test_part2.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) worklet1/test_part2.c -o test_worklet1_part2

test_worklet1_part3: worklet1/lexer.c worklet1/test_part3.c
	$(CC) $(DEBUG_FLAGS) worklet1/lexer.c worklet1/test_part3.c -o test_worklet1_part3

test_worklet1_part4: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W1_PARSE) worklet1/test_part4.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W1_PARSE) worklet1/test_part4.c -o test_worklet1_part4

test_worklet3_part5: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) worklet3/test_part5.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) worklet3/test_part5.c -o test_worklet3_part5

test_worklet3_part6: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) worklet3/test_part6.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) worklet3/test_part6.c -o test_worklet3_part6

test_worklet3_part7: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) worklet3/test_part7.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) worklet3/test_part7.c -lm -o test_worklet3_part7

test_worklet3_part8: $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) worklet3/test_part8.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) worklet3/test_part8.c -lm -o test_worklet3_part8

test_worklet1_part9: calculator $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) worklet1/test_part9.c
	$(CC) $(DEBUG_FLAGS) $(SRC_W2) $(SRC_W1_IO) $(SRC_W3_MATH) $(SRC_W3_DEC) $(SRC_W3_FUNC) $(SRC_W1_PARSE) $(SRC_W3_EVAL) $(SRC_W1_REPL) worklet1/test_part9.c -lm -o test_worklet1_part9

run_tests: test_worklet2 test_worklet1_part2 test_worklet1_part3 test_worklet1_part4 test_worklet3_part5 test_worklet3_part6 test_worklet3_part7 test_worklet3_part8 test_worklet1_part9 test_part10
	@echo "Running Worklet 2 / Part 1 Tests:"
	./test_worklet2
	@echo ""
	@echo "Running Worklet 1 / Part 2 Tests:"
	./test_worklet1_part2
	@echo ""
	@echo "Running Worklet 1 / Part 3 Tests:"
	./test_worklet1_part3
	@echo ""
	@echo "Running Worklet 1 / Part 4 Tests:"
	./test_worklet1_part4
	@echo ""
	@echo "Running Worklet 3 / Part 5 Tests:"
	./test_worklet3_part5
	@echo ""
	@echo "Running Worklet 3 / Part 6 Tests:"
	./test_worklet3_part6
	@echo ""
	@echo "Running Worklet 3 / Part 7 Tests:"
	./test_worklet3_part7
	@echo ""
	@echo "Running Worklet 3 / Part 8 Tests:"
	./test_worklet3_part8
	@echo ""
	@echo "Running Worklet 1 / Part 9 Tests:"
	./test_worklet1_part9
	@echo ""
	@echo "Running Comprehensive Part 10 Verification Suite:"
	./test_part10

clean:
	rm -f calculator test_worklet2 test_worklet1_part2 test_worklet1_part3 test_worklet1_part4 test_worklet3_part5 test_worklet3_part6 test_worklet3_part7 test_worklet3_part8 test_worklet1_part9 test_part10 test_part1 *.o repl_output.txt

.PHONY: all clean run_tests
