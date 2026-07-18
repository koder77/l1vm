# Brackets Code

A CLI code generator for the Brackets (L1VM) language — v0.6.7.
Converts natural-language prompts into working `.l1com` programs.

## How it works

- **197 code generation blocks** — 31 template-matched + 166 plan-based emitters with vector scoring
- **Three-tier pipeline**: smart planner runs first, template matching as fallback, default "hello world" last
- **No LLM involved** — deterministic pattern extraction from `l1vm-example-code/`
- **Vector search** over example code with `--search` (interactive: `/search`)
- **Learn/forget** user-provided patterns at runtime without recompilation

## Requirements

- Clang or GCC with math library (`-lm`)
- L1VM toolchain: `l1pre`, `l1com`, `l1asm`, l1vm from L1VM_ROOT environment variable pointing to the L1VM installation.

## Build

    make

Or manually:

    clang -O2 -Wall -Wextra -o brackets-code brackets-code.c dsl.c -lm

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
Learn DSL rule:       brackets-code --learn-dsl <keyword> [options]
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
    --learn-dsl <kw> [opts]  Learn/save DSL rule (see DSL Learning below)
    --forget <id>            Forget a learned pattern
    --list-learned           List learned patterns
    --verbose                Show emitter selection scores
    --dry-run                Print generated code to stdout (don't write file)
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

    ./tests/run_tests.sh          # run all tests
    ./tests/run_tests.sh quick    # quick subset (14 tests)
    ./tests/run_tests.sh list     # list test names
    ./tests/run_tests.sh save <d> # save failing sources to <d>

## Project structure

    brackets-code.c        Main source (~3280 lines)
    brackets-code.h        Header file with types and declarations (535 lines)
    brackets-code          Compiled binary
    dsl.c                  DSL rule engine (separately compiled, linked)
    dsl.h                  DSL header
    embed.c                Tiny LLM inference engine + vector search (separately compiled, linked)
    learn.c                Learned pattern system (separately compiled, linked)
    synonyms.txt           External synonym table (edit without recompilation)
    brackets-code.1        Man page
    dsl/                   162 .l1dsl DSL rule files
    l1vm-example-code/     Reference L1VM programs (pattern source)
    tests/run_tests.sh     Test suite
    tests/test_dsl_rules.sh DSL rule test pipeline
    tests/test_unit.c      Unit tests for C functions
    tests/saved-failures/  Saved failing test outputs
    memory.txt             Architecture documentation

## DSL Learning

DSL rules (`.l1dsl` files) define how natural-language prompts map to L1VM code.
Each rule has: `parser:` keywords, `token:` declarations, `include:` files,
`desc:` description, and a `code:` block.

`--learn-dsl` lets you persist DSL rules — either already in-memory rules or
brand-new ones created from the command line.

### Save an existing in-memory rule

If a rule was created at runtime (e.g. via `dsl_add_array_rule()`), save it:

    brackets-code --learn-dsl "array scores"

This writes `dsl/array-scores.l1dsl`. On next startup `dsl_load_rules("dsl")`
picks it up automatically.

### Create and save a new rule

Build a rule from CLI flags and a code file:

    brackets-code --learn-dsl "my rule" \
        --token int64 n \
        --include intr-func.l1h \
        --desc "Description of what it does" \
        --code my-code.l1com

The `--code` file must contain a `code:` block. Everything after that line until
the next header (`parser:`, `token:`, etc.) or EOF is taken as code.

Flags:

    --token <type> <name>   Add a token (int64, double, string, const-int64, ...)
    --include <file>        Add an include (e.g. intr-func.l1h, vars.l1h)
    --desc <text>           Set description
    --code <file>           Read code from a .l1com file (must have code: block)
    --out-dir <dir>         Output directory (default: dsl/)

### Example: teaching a new pattern

Create a code file `hello-code.l1com`:

    code:
    (set string 6 msg "hello!")
    (msg :print_s !)
    (:print_n !)

Then save it as a DSL rule:

    brackets-code --learn-dsl "say hello" \
        --include intr-func.l1h \
        --desc "Print hello message" \
        --code hello-code.l1com

This creates `dsl/say-hello.l1dsl`:

    // say hello
    parser: "say hello"
    desc: "Print hello message"
    include: intr-func.l1h

    code:
    (set string 6 msg "hello!")
    (msg :print_s !)
    (:print_n !)

Next time someone runs `brackets-code "say hello"`, the new rule matches
and generates the code automatically.

### How it fits in the pipeline

    Program start
      → dsl_load_rules("dsl")     loads all .l1dsl files
      → user prompt arrives
      → dsl_match_rule()           finds matching rule by keyword
      → dsl_generate_code()        emits L1VM code from rule

    With learn-dsl:
      → runtime creates new rule (e.g. dsl_add_array_rule)
      → user: brackets-code --learn-dsl "keyword"
      → rule persisted to dsl/<name>.l1dsl
      → next startup: rule loaded automatically

## License

GPL
