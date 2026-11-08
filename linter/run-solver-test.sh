#!/bin/bash
# Regression test for the optional Z3 bounded model checker.
# Usage: ./run-solver-test.sh [path-to-l1vm-linter]
#
# When the linter was compiled without Z3 the test is skipped, because the
# solver pass is an optional feature.

set -u

here="$(cd "$(dirname "$0")" && pwd)"
linter="${1:-$here/l1vm-linter}"
program="$here/linter-solver-test.l1com"

if [ ! -x "$linter" ]; then
    echo "error: linter executable not found: $linter" >&2
    echo "build it first with: CC=gcc ./make.sh" >&2
    exit 1
fi

# Z3 support is compiled in when the binary references the Z3 C API. This
# also works for statically linked builds and on platforms with a leading
# underscore in symbol names.
if ! (nm "$linter" 2>/dev/null || strings "$linter" 2>/dev/null) | grep -q 'Z3_mk_context'; then
    echo "Z3 solver support not compiled in; skipping solver regression test"
    exit 0
fi

output="$("$linter" "$program" 2>&1)"
status=$?
rm -f "$program.l1lint"

expected=(
    "solver: assertion is always false at this point"
    "solver: condition is always true, the else branch is dead code"
    "solver: division by zero on every feasible path"
    "solver: shift amount is outside [0, 63] on every feasible path"
)

failed=0
for text in "${expected[@]}"; do
    if ! grep -qF "$text" <<< "$output"; then
        echo "MISSING: $text" >&2
        failed=1
    fi
done

# Exactly the four findings above must be emitted. Extra solver warnings mean
# the solver produced a false positive or a duplicate.
count="$(grep -cF 'solver:' <<< "$output")"
if [ "$count" -ne 4 ]; then
    echo "error: expected 4 solver diagnostics but got $count" >&2
    failed=1
fi

# Lines which follow from the constraints must not be reported.
if grep -A1 -F 'solver:' <<< "$output" | grep -qF '(rel_y 6 ==)'; then
    echo "error: solver reported a line which is provably safe" >&2
    failed=1
fi

# A conditional jump may bypass the assignment, so this assertion has a
# feasible path and must not be reported.
if grep -A1 -F 'solver:' <<< "$output" | grep -qF '(jump_x seven ==)'; then
    echo "error: solver ignored an unmodelled jump predecessor" >&2
    failed=1
fi

# A pointer write may change the scalar through an alias.
if grep -A1 -F 'solver:' <<< "$output" | grep -qF '(alias_x seven ==)'; then
    echo "error: solver ignored a possible pointer alias" >&2
    failed=1
fi

if [ "$status" -eq 0 ]; then
    echo "error: linter returned success but solver warnings were expected" >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    echo "--- linter output ---" >&2
    echo "$output" >&2
    exit 1
fi

echo "solver regression test passed"
exit 0
