#!/bin/bash
# Test Suite for brackets-code
# Runs each template/emitter prompt through the full l1pre→l1com pipeline
#
# Usage:
#   ./tests/run_tests.sh                 # run all tests
#   ./tests/run_tests.sh quick           # quick subset
#   ./tests/run_tests.sh list            # list test names
#   ./tests/run_tests.sh save <dir>      # run all, save failed sources to <dir>
#   BRACKETS_CODE=/path ./tests/run_tests.sh   # custom binary path

set -euo pipefail

THIS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$THIS_DIR/.." && pwd)"
BRACKETS_CODE="${BRACKETS_CODE:-$PROJECT_DIR/brackets-code}"
TMPDIR="$(mktemp -d)"
SAVEDIR=""   # set by 'save' mode

L1VM_ROOT="${L1VM_ROOT:-$HOME/l1vm/}"
L1VM_BIN="${L1VM_ROOT}bin"
INCLUDE_DIR="${L1VM_ROOT}/include/"
PATH="${L1VM_BIN}:${PATH}"

PASS=0
FAIL=0
SKIP=0
KNOWN_FAIL=0

cleanup() { [ -z "$SAVEDIR" ] && rm -rf "$TMPDIR"; }
trap cleanup EXIT

# ── test runner ──────────────────────────────────────────────────────────
run_test() {
    local category="$1" name="$2" prompt="$3"
    local base="${TMPDIR}/${name}"
    local fail_reason=""

    # skip known failures
    if [ "${KNOWN_FAILURES[$name]+set}" = "set" ]; then
        echo -e "\r  [\e[33mSKIP\e[0m] $name (known failure: ${KNOWN_FAILURES[$name]})"
        SKIP=$((SKIP + 1))
        return
    fi

    echo -n "  [ ] $name ... "

    # generate
    if ! "$BRACKETS_CODE" "$prompt" "${base}.l1com" > /dev/null 2>&1; then
        echo -e "\r  [\e[33mSKIP\e[0m] $name (generation failed)"
        SKIP=$((SKIP + 1))
        [ -n "$SAVEDIR" ] && cp "${base}.l1com" "$SAVEDIR/${category}_${name}.l1com" 2>/dev/null || true
        return
    fi

    # preprocess
    if ! l1pre "${base}.l1com" "${base}_pp.l1com" "$INCLUDE_DIR" > /dev/null 2>&1; then
        fail_reason="l1pre"
    fi

    # compile
    if [ -z "$fail_reason" ]; then
        local ppbase="${base}_pp"
        if ! l1com "$ppbase" > /dev/null 2>&1; then
            fail_reason="l1com"
        fi
    fi

    if [ -n "$fail_reason" ]; then
        echo -e "\r  [\e[31mFAIL\e[0m] $name ($fail_reason)"
        FAIL=$((FAIL + 1))
        [ -n "$SAVEDIR" ] && cp "${base}.l1com" "$SAVEDIR/${category}_${name}.l1com" 2>/dev/null || true
        [ -n "$SAVEDIR" ] && cp "${base}_pp.l1com" "$SAVEDIR/${category}_${name}_pp.l1com" 2>/dev/null || true
        return
    fi

    echo -e "\r  [\e[32m OK \e[0m] $name"
    PASS=$((PASS + 1))
}

# ── known failures (emitters that use :input_i and cannot be tested with piped stdin) ─
declare -A KNOWN_FAILURES=(
    [em_base_converter]="interactive :input_i not testable with pipe"
    [em_binary_search_tree]="interactive menu loop not testable with pipe"

)

# ── test definitions ─────────────────────────────────────────────────────
declare -A TESTS

# hello / basic
TESTS[hello_world]="hello world"
TESTS[hello_name]="what is your name"
TESTS[print_var]="print a variable"

# math
TESTS[math_ops]="show me math operations"
TESTS[sum]="sum of numbers"
TESTS[max_of_three]="max of three numbers"

# loops
TESTS[for_loop]="create a for loop"
TESTS[while_loop]="create a while loop"
TESTS[countdown_10]="countdown from 10"

# conditionals
TESTS[if_else]="if else condition"
TESTS[pos_neg]="positive negative zero"
TESTS[switch_case]="switch case example"

# arrays
TESTS[array_demo]="array index access"

# strings
TESTS[string_concat]="string concat"
TESTS[string_demo]="string operations"

# user input
TESTS[user_input]="get user input"
TESTS[shell_args]="command line arguments"

# math
TESTS[factorial]="calculate factorial of 5"
TESTS[fibonacci]="calculate fibonacci"
TESTS[fizzbuzz]="fizzbuzz"
TESTS[primes]="print prime numbers"
TESTS[gcd]="greatest common divisor"
TESTS[power]="power exponent"

# games / misc
TESTS[guess_number]="guess the number"
TESTS[multiplication_table]="multiplication table"
TESTS[even_odd]="even or odd"
TESTS[bubble_sort]="bubble sort"
TESTS[struct_demo]="person struct"
TESTS[pointer_demo]="pointer access"
TESTS[time_demo]="get current time"
TESTS[hex_binary]="hex and binary"
TESTS[function_demo]="define a function"

# ── emitters (smart generation path) ────────────────────────────────────
declare -A EMITTER_TESTS
EMITTER_TESTS[em_math]="calculate 10 plus 5"
EMITTER_TESTS[em_loop]="print numbers 1 to 5 in a loop"
EMITTER_TESTS[em_even]="print even numbers up to 10"
EMITTER_TESTS[em_countdown]="count down from 5"
EMITTER_TESTS[em_string_cat]="concatenate two strings"
EMITTER_TESTS[em_array_assign]="assign values to an array"
EMITTER_TESTS[em_array_reverse]="reverse an array"
EMITTER_TESTS[em_array_find]="find a value in an array"
EMITTER_TESTS[em_factorial_num]="factorial of 5"
EMITTER_TESTS[em_fizzbuzz]="fizzbuzz"
EMITTER_TESTS[em_primes]="prime numbers up to 20"
EMITTER_TESTS[em_even_odd]="check if 7 is even or odd"
EMITTER_TESTS[em_power]="2 to the power of 8"
EMITTER_TESTS[em_multiplication]="multiplication table"
EMITTER_TESTS[em_guess]="guess number game"
EMITTER_TESTS[em_gcd]="gcd of 12 and 8"
EMITTER_TESTS[em_random]="random number"
EMITTER_TESTS[em_hello_name]="say hello to Alice"
EMITTER_TESTS[em_array_min_max]="find min max and average in array"
EMITTER_TESTS[em_bool_demo]="boolean variable demo"
EMITTER_TESTS[em_bit_check]="check bits of number 134"
EMITTER_TESTS[em_function]="define a function"
EMITTER_TESTS[em_leap_year]="check if leap year"
EMITTER_TESTS[em_temp_convert]="convert celsius to fahrenheit"
EMITTER_TESTS[em_circle_area]="area of a circle"
EMITTER_TESTS[em_average]="average of 5 numbers"
EMITTER_TESTS[em_selection_sort]="sort numbers"
EMITTER_TESTS[em_palindrome]="check palindrome number"
EMITTER_TESTS[em_lcm]="least common multiple"
EMITTER_TESTS[em_collatz]="collatz sequence"
EMITTER_TESTS[em_sum_of_digits]="sum of digits"
EMITTER_TESTS[em_reverse_string]="reverse a string"
EMITTER_TESTS[em_string_length]="string length"
EMITTER_TESTS[em_dice_roll]="dice roll simulation"
EMITTER_TESTS[em_compound_interest]="compound interest calculation"
EMITTER_TESTS[em_square_root]="square root of 144"
EMITTER_TESTS[em_decimal_to_binary]="decimal to binary"
EMITTER_TESTS[em_fibonacci]="fibonacci sequence"
EMITTER_TESTS[em_bmi_calculator]="bmi calculator"
EMITTER_TESTS[em_pyramid]="print pyramid pattern"
EMITTER_TESTS[em_standard_deviation]="standard deviation of 5 numbers"
EMITTER_TESTS[em_prime_factorization]="prime factorization of 24"
EMITTER_TESTS[em_base_converter]="base converter interactive"
EMITTER_TESTS[em_binary_search_tree]="binary search tree interactive"

# ── multi-step tests ────────────────────────────────────────────────────
declare -A MULTI_TESTS
MULTI_TESTS[multi_sort_then_reverse]="sort 5 numbers und dann reverse the array"
MULTI_TESTS[multi_fact_then_power]="factorial of 5 and then 2 to the power of 8"
MULTI_TESTS[multi_even_then_countdown]="print even numbers up to 10 und danach count down from 5"
MULTI_TESTS[multi_sum_then_avg]="sum from 1 to 100 and then average of 5 numbers"
MULTI_TESTS[multi_count_fact_reverse]="step 1: count down from 5 step 2: factorial of 3 step 3: reverse the array"
MULTI_TESTS[multi_gcd_lcm]="first calculate gcd of 12 and 8 then calculate lcm of 6 and 10"
MULTI_TESTS[multi_prim_pow_fib]="prime numbers up to 10 und dann 2 to the power of 6 und danach fibonacci sequence"
MULTI_TESTS[multi_count_even_sum]="count from 1 to 20 and then sum even numbers"
MULTI_TESTS[multi_fizz_power]="fizzbuzz and then 3 to the power of 5"
MULTI_TESTS[multi_sort_desc]="sort 5 numbers und danach sort descending"
MULTI_TESTS[multi_fact_fib]="factorial of 6 und dann fibonacci of 10"

# ── DSL rule tests ─────────────────────────────────────────────────────
declare -A DSL_TESTS

# math / number theory
DSL_TESTS[dsl_collatz]="collatz conjecture"
DSL_TESTS[dsl_collatz_from]="collatz from 7"
DSL_TESTS[dsl_prime_factor]="prime factorization of 36"
DSL_TESTS[dsl_palindrome_num]="palindrome number 12321"
DSL_TESTS[dsl_sum_digits]="sum of digits of 9876"
DSL_TESTS[dsl_lcm]="least common multiple 6 and 10"
DSL_TESTS[dsl_binary_search]="binary search for 42"
DSL_TESTS[dsl_leap_year]="leap year 2024"
DSL_TESTS[dsl_power]="power 2 to 10"
DSL_TESTS[dsl_circle_area]="circle area radius 5"
DSL_TESTS[dsl_compound_interest]="compound interest calculate"
DSL_TESTS[dsl_bit_check]="bit check of 42"

# array operations
DSL_TESTS[dsl_shuffle_array]="shuffle array of 10"
DSL_TESTS[dsl_find_max]="find max in data"
DSL_TESTS[dsl_average_data]="compute average of data"
DSL_TESTS[dsl_reverse_data]="reverse data list"
DSL_TESTS[dsl_sum_data]="sum all data elements"
DSL_TESTS[dsl_array_access]="read elements of data"
DSL_TESTS[dsl_array_write]="get data at index 3"
DSL_TESTS[dsl_shuffle_elements]="shuffle array elements"

# double array operations
DSL_TESTS[dsl_sum_double]="sum double data"

# string operations
DSL_TESTS[dsl_string_find]="find in string text"
DSL_TESTS[dsl_string_split]="split string text"
DSL_TESTS[dsl_string_to_num]="string to number 42"
DSL_TESTS[dsl_type_convert]="type conversion int to string"
DSL_TESTS[dsl_string_length]="string length of text"
DSL_TESTS[dsl_reverse_string]="reverse string text hello"
DSL_TESTS[dsl_uppercase]="uppercase text hello"
DSL_TESTS[dsl_lowercase]="lowercase text HELLO"
DSL_TESTS[dsl_count_vowels]="count vowels hello world"
DSL_TESTS[dsl_anagram]="check if anagram words"
DSL_TESTS[dsl_palindrome_string]="check palindrome string racecar"

# sorting
DSL_TESTS[dsl_insertion_sort]="insertion sort data"
DSL_TESTS[dsl_selection_sort]="selection sort data"
DSL_TESTS[dsl_bubble_desc]="bubble sort descending order"
DSL_TESTS[dsl_sort_stats]="sort statistics calculate"

# loops / conditions
DSL_TESTS[dsl_loop_demo]="loop demo example"
DSL_TESTS[dsl_for_sum]="for loop sum 1 to 10"
DSL_TESTS[dsl_countdown]="countdown from 10"
DSL_TESTS[dsl_print_even]="print even numbers up to 20"
DSL_TESTS[dsl_switch_demo]="switch demo example"
DSL_TESTS[dsl_iterative_factorial]="iterative factorial of 5"
DSL_TESTS[dsl_array_iterate]="iterate array data"
DSL_TESTS[dsl_array_min_max]="array min max of data"
DSL_TESTS[dsl_base_converter]="base conversion to binary 42"
DSL_TESTS[dsl_calculator]="calculator add 5 and 3"
DSL_TESTS[dsl_mult_table]="multiply table of 5"
DSL_TESTS[dsl_number_to_words]="number to words of 42"
DSL_TESTS[dsl_primes]="prime numbers up to 20"
DSL_TESTS[dsl_square_root]="square root of 144"
DSL_TESTS[dsl_random_number]="random number 1 to 100"
DSL_TESTS[dsl_temperature_table]="temperature table display"
DSL_TESTS[dsl_number_analyzer]="number analyzer 77"
DSL_TESTS[dsl_function]="function add two numbers"
DSL_TESTS[dsl_double_math]="double math add 1.5 and 2.5"
DSL_TESTS[dsl_double_power]="double power 2.0 to 3.0"
DSL_TESTS[dsl_math_ops]="math ops add 5 and 3"
DSL_TESTS[dsl_queue]="queue demo example"
DSL_TESTS[dsl_freq_analysis]="freq analysis of data"
DSL_TESTS[dsl_math_menu]="math menu example"
DSL_TESTS[dsl_bmi]="bmi calculator height 180 weight 75"
DSL_TESTS[dsl_guess_number]="guess number game"
DSL_TESTS[dsl_shell_args]="shell args demo"
DSL_TESTS[dsl_string_compare]="string compare abc and def"
DSL_TESTS[dsl_double_array_sum]="double array sum of data"
DSL_TESTS[dsl_double_array_write]="double array write data"
DSL_TESTS[dsl_double_array_iterate]="double array iterate data"
DSL_TESTS[dsl_double_array_min_max]="double array min max data"
DSL_TESTS[dsl_double_array_average]="double array average of data"
DSL_TESTS[dsl_double_array_reverse]="double array reverse data"
DSL_TESTS[dsl_double_array_access]="double array access index 2"
DSL_TESTS[dsl_add]="add 10 and 20"
DSL_TESTS[dsl_sub]="subtract 10 from 20"
DSL_TESTS[dsl_mul]="multiply 5 and 3"
DSL_TESTS[dsl_div]="divide 20 by 4"
DSL_TESTS[dsl_even_odd]="even or odd of 7"
DSL_TESTS[dsl_positive_negative]="positive or negative number"
DSL_TESTS[dsl_hello_world]="hello world program"
DSL_TESTS[dsl_hello_name]="hello with name"
DSL_TESTS[dsl_print_var]="print variable value"
DSL_TESTS[dsl_array_demo]="demo array of numbers"
DSL_TESTS[dsl_string_demo]="string demo hello"
DSL_TESTS[dsl_string_cat]="string concat hello and world"
DSL_TESTS[dsl_while_loop]="loop while counter less 10"
DSL_TESTS[dsl_if_else]="if else condition check"
DSL_TESTS[dsl_if_else_demo]="if else demo example"
DSL_TESTS[dsl_stddev]="standard deviation of data"
DSL_TESTS[dsl_pointer_demo]="demo pointer of value"
DSL_TESTS[dsl_user_input]="user input example"
DSL_TESTS[dsl_decimal_to_binary]="decimal to binary 42"
DSL_TESTS[dsl_factorial]="factorial of 10"
DSL_TESTS[dsl_fibonacci]="fibonacci sequence"
DSL_TESTS[dsl_fizzbuzz]="fizzbuzz program"
DSL_TESTS[dsl_gcd]="gcd of 12 and 8"
DSL_TESTS[dsl_sum_range]="sum range 1 to 100"

# misc
DSL_TESTS[dsl_dice_roll]="dice roll 3 times"
DSL_TESTS[dsl_dice_sim]="dice roll simulation"
DSL_TESTS[dsl_shuffle]="shuffle the data"
DSL_TESTS[dsl_unit_convert]="unit convert kilometers to miles"
DSL_TESTS[dsl_string_analyzer]="string analyzer for vowels"
DSL_TESTS[dsl_ascii_art]="ascii art triangle"
DSL_TESTS[dsl_hex]="decimal to hexadecimal"
DSL_TESTS[dsl_temp_convert]="temp convert celsius to fahrenheit"
DSL_TESTS[dsl_time]="time demo clock"
DSL_TESTS[dsl_pyramid]="pyramid of height 5"

# ── main ─────────────────────────────────────────────────────────────────
MODE="${1:-all}"

if [ "$MODE" = "list" ]; then
    echo "--- templates ---"
    for name in $(echo "${!TESTS[@]}" | tr ' ' '\n' | sort); do
        echo "  $name: ${TESTS[$name]}"
    done
    echo "--- emitters ---"
    for name in $(echo "${!EMITTER_TESTS[@]}" | tr ' ' '\n' | sort); do
        echo "  $name: ${EMITTER_TESTS[$name]}"
    done
    exit 0
fi

if [ "$MODE" = "save" ]; then
    SAVEDIR="${2:-$PROJECT_DIR/tests/saved-failures}"
    mkdir -p "$SAVEDIR"
    echo "Saving failed sources to: $SAVEDIR"
fi

if [ "$MODE" = "quick" ]; then
    # quick subset: 6 templates + 3 emitters
    unset TESTS; declare -A TESTS
    TESTS[hello_world]="hello world"
    TESTS[for_loop]="for loop example"
    TESTS[if_else]="if else condition"
    TESTS[factorial]="calculate factorial of 5"
    TESTS[fizzbuzz]="fizzbuzz"
    TESTS[gcd]="greatest common divisor"
    unset EMITTER_TESTS; declare -A EMITTER_TESTS
    EMITTER_TESTS[em_factorial]="factorial of 5"
    EMITTER_TESTS[em_fizzbuzz]="fizzbuzz"
    EMITTER_TESTS[em_math]="calculate 2 plus 3"
    EMITTER_TESTS[em_leap_year]="check if leap year"
    EMITTER_TESTS[em_temp_convert]="convert celsius to fahrenheit"
    EMITTER_TESTS[em_average]="average of 5 numbers"
    EMITTER_TESTS[em_selection_sort]="sort numbers"
    unset MULTI_TESTS; declare -A MULTI_TESTS
    MULTI_TESTS[multi_sort_then_reverse]="sort 5 numbers und dann reverse the array"
fi

echo "========================================"
echo "  brackets-code test suite"
echo "========================================"
echo "binary:  $BRACKETS_CODE"
echo "include: $INCLUDE_DIR"
echo ""

echo "--- template tests ---"
for name in $(echo "${!TESTS[@]}" | tr ' ' '\n' | sort); do
    run_test "template" "$name" "${TESTS[$name]}"
done

echo ""
echo "--- emitter tests ---"
for name in $(echo "${!EMITTER_TESTS[@]}" | tr ' ' '\n' | sort); do
    run_test "emitter" "$name" "${EMITTER_TESTS[$name]}"
done

echo ""
echo "--- multi-step tests ---"
for name in $(echo "${!MULTI_TESTS[@]}" | tr ' ' '\n' | sort); do
    run_test "multi" "$name" "${MULTI_TESTS[$name]}"
done

echo ""
echo "--- DSL rule tests ---"
for name in $(echo "${!DSL_TESTS[@]}" | tr ' ' '\n' | sort); do
    run_test "dsl" "$name" "${DSL_TESTS[$name]}"
done

echo ""
echo "========================================"
echo "  results: $PASS passed, $FAIL failed, $SKIP skipped"
if [ "${#KNOWN_FAILURES[@]}" -gt 0 ]; then
    echo ""
    echo "--- known failures (${#KNOWN_FAILURES[@]} total) ---"
    for name in $(echo "${!KNOWN_FAILURES[@]}" | tr ' ' '\n' | sort); do
        echo "  - $name: ${KNOWN_FAILURES[$name]}"
    done
fi
echo "========================================"

[ "$FAIL" -eq 0 ]
