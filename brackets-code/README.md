# Brackets Code

A CLI code generator for the Brackets (L1VM) language.
Converts natural-language prompts into working `.l1com` programs.

## How it works

- **70 code generation blocks** — 31 templates (keyword-matched) + 39 emitters (plan-based via tiny word embedding scorer)
- **Two-tier pipeline**: smart planner runs first, template matching as fallback, default "hello world" last
- **No LLM involved** — deterministic pattern extraction from `l1vm-example-code/`
- **55/55 tests pass** through the full `l1pre → l1com` compilation pipeline

## Requirements

- GCC with math library (`-lm`)
- L1VM toolchain: `l1pre`, `l1com`, `l1asm`, l1vm from L1VM_ROOT environment variable pointing to the L1VM installation.

## Build

    make

Or manually:

    gcc -o brackets-code brackets-code.c -lm

## Usage

    brackets-code <prompt> [filename]

Examples:

    brackets-code "hello world"
    brackets-code "calculate factorial of 7" fact.l1com
    brackets-code "read 5 numbers, sort them, and print the largest"
    brackets-code --validate "fizzbuzz"

Interactive mode (no arguments):

    brackets-code

Flags:

    --help / -h      Show help
    --list / -l      List all templates
    --validate / -v  Generate and compile-check via l1pre → l1com

## Testing

    make test

Or:

    ./tests/run_tests.sh          # run all 55 tests
    ./tests/run_tests.sh quick    # quick subset (9 tests)
    ./tests/run_tests.sh list     # list test names
    ./tests/run_tests.sh save <d> # save failing sources to <d>

## Project structure

    brackets-code.c        Main source (~5300 lines)
    brackets-code          Compiled binary
    l1vm-example-code/     Reference L1VM programs (pattern source)
    tests/run_tests.sh     Test suite
    memory.txt             Architecture documentation

## License

GPL
