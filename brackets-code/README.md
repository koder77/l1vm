# Brackets Code

A CLI code generator for the Brackets (L1VM) language — v0.6.2.
Converts natural-language prompts into working `.l1com` programs.

## How it works

- **91 code generation blocks** — 31 templates (keyword-matched) + 60 emitters (plan-based via tiny word embedding scorer)
- **Three-tier pipeline**: smart planner runs first, template matching as fallback, default "hello world" last
- **No LLM involved** — deterministic pattern extraction from `l1vm-example-code/`
- **59/59 tests pass** through the full `l1pre → l1com` compilation pipeline
- **Vector search** over example code with `--search` (interactive: `/search`)
- **Learn/forget** user-provided patterns at runtime without recompilation

## Requirements

- GCC with math library (`-lm`)
- L1VM toolchain: `l1pre`, `l1com`, `l1asm`, l1vm from L1VM_ROOT environment variable pointing to the L1VM installation.

## Build

    make

Or manually:

    gcc -o brackets-code brackets-code.c -lm

## Usage

```
One-shot:             brackets-code "<prompt>" [filename]
Pipe mode:            echo "<prompt>" | brackets-code - [filename]
Interactive mode:     brackets-code
With validation:      brackets-code --validate "<prompt>" [filename]
Self-test:            brackets-code --self-test
Batch mode:           brackets-code --batch prompts.txt
Vector search:        brackets-code --search "<query>"
Learn from file:      brackets-code --learn <file.l1com> [keywords] ["description"]
Forget pattern:       brackets-code --forget <pattern-id>
List learned:         brackets-code --list-learned
```

Examples:

    brackets-code "hello world"
    brackets-code "calculate factorial of 7" fact.l1com
    brackets-code "read 5 numbers, sort them, and print the largest"
    brackets-code --validate "fizzbuzz"
    brackets-code --batch prompts.txt
    brackets-code --search "sort numbers"

Flags:

    --help / -h              Show help
    --list / -l              List all templates
    --validate / -v          Generate and compile-check via l1pre → l1com
    --self-test              Run built-in self-test
    --batch <file>           Process prompts line-by-line from a file
    --search "<query>"       Vector search example code
    --learn <f> [kw] [desc]  Learn new pattern from .l1com file
    --forget <id>            Forget a learned pattern
    --list-learned           List learned patterns
    --verbose                Show emitter selection scores
    --out-dir <dir>          Output directory for generated files
    --l1vm-root <path>       L1VM installation root (for --validate)
    --bash-completion        Print bash completion script

## Interactive mode

Run without arguments for an interactive REPL. Special commands:

    /help                    Show help
    /list                    List all templates
    /learn <f> <kw> [desc]  Learn pattern from .l1com file
    /forget <id>             Forget a learned pattern
    /list-learned /ll        List learned patterns
    /search <q>              Vector search example code
    /exit                    Exit
    /save <fn>               Save last generated code to file

Tab completion is supported.

## Testing

    make test

Or:

    ./tests/run_tests.sh          # run all 59 tests
    ./tests/run_tests.sh quick    # quick subset (9 tests)
    ./tests/run_tests.sh list     # list test names
    ./tests/run_tests.sh save <d> # save failing sources to <d>

## Project structure

    brackets-code.c        Main source (~7762 lines)
    brackets-code          Compiled binary
    l1vm-example-code/     Reference L1VM programs (pattern source)
    tests/run_tests.sh     Test suite
    memory.txt             Architecture documentation

## License

GPL
