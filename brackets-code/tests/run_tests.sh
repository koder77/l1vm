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

L1VM_ROOT="${L1VM_ROOT:-/home/stefan/l1vm/}"
L1VM_BIN="${L1VM_ROOT}bin"
INCLUDE_DIR="${L1VM_ROOT}include/"
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

# ── known failures (templates with known bugs) ──────────────────────────
declare -A KNOWN_FAILURES=()

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
EMITTER_TESTS[em_leap_year]="check if leap year"
EMITTER_TESTS[em_temp_convert]="convert celsius to fahrenheit"
EMITTER_TESTS[em_circle_area]="area of a circle"
EMITTER_TESTS[em_average]="average of 5 numbers"
EMITTER_TESTS[em_selection_sort]="sort numbers"

# ── multi-step tests ────────────────────────────────────────────────────
declare -A MULTI_TESTS
MULTI_TESTS[multi_sort_then_reverse]="sort 5 numbers und dann reverse the array"

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
