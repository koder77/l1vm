#!/bin/bash
# Regression test for the CBMC-style logical bug diagnostics.
# Usage: ./run-logic-test.sh [path-to-l1vm-linter]

set -u

here="$(cd "$(dirname "$0")" && pwd)"
linter="${1:-$here/l1vm-linter}"
program="$here/linter-logic-test.l1com"

if [ ! -x "$linter" ]; then
    echo "error: linter executable not found: $linter" >&2
    echo "build it first with: CC=gcc ./make.sh" >&2
    exit 1
fi

output="$("$linter" "$program" 2>&1)"
status=$?
rm -f "$program.l1lint"

expected=(
    "condition is always true, the else branch is dead code"
    "condition is always false, the then branch is dead code"
    "assertion is always false at this point"
    "assumption can never hold at this point"
    "assignment to constant variable 'zero'"
    "self-assignment of variable 'x' (no-op)"
    "unreachable code (control flow ends before this line)"
    "duplicate switch case value 1"
    "function 'inc' can reach (funcend) without a return value"
    "interprocedural: in function 'divarg' called from 'main': division by zero"
    "interprocedural: in function 'inner': division by zero"
    "interprocedural: in function 'arrow->div' called from 'main': division by zero"
    "division by zero"
    "shift amount out of range"
)

failed=0
for text in "${expected[@]}"; do
    if ! grep -qF "$text" <<< "$output"; then
        echo "MISSING: $text" >&2
        failed=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "error: linter returned success but warnings were expected" >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    echo "--- linter output ---" >&2
    echo "$output" >&2
    exit 1
fi

echo "logical bug regression test passed"
exit 0
