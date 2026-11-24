#!/usr/bin/env bash
# Build every Lisp test program in this directory.
#
#   ./build.sh              compile .lisp -> .l1com with clisp lisp2l1vm.lisp
#   ./build.sh --vm         also assemble the .l1com files with l1vm-build.sh
#   ./build.sh --run        build and run them through the L1VM
#
# Environment overrides: CLISP, L1VM_BUILD, L1VM, L1VM_MODULES.

set -u

here="$(cd "$(dirname "$0")" && pwd)"
compiler="$here/../lisp2l1vm.lisp"
clispbin="${CLISP:-clisp}"
vmbuild="${L1VM_BUILD:-l1vm-build.sh}"
vm="${L1VM:-l1vm}"
modules="${L1VM_MODULES:-/home/stefan/develop/l1vm/modules}"

do_vm=0
do_run=0
for arg in "$@"; do
    case "$arg" in
        --vm)  do_vm=1 ;;
        --run) do_vm=1; do_run=1 ;;
        -h|--help)
            printf 'usage: %s [--vm] [--run]\n' "$0"
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n' "$arg" >&2
            exit 2
            ;;
    esac
done

if [ ! -f "$compiler" ]; then
    printf 'compiler not found: %s\n' "$compiler" >&2
    exit 2
fi

cd "$here" || exit 2

failed=0
for src in *.lisp; do
    [ -e "$src" ] || continue
    name="${src%.lisp}"
    printf 'compiling %-16s -> %s.l1com ... ' "$src" "$name"
    if "$clispbin" "$compiler" "$src" "$name.l1com" >"$name.compile.log" 2>&1; then
        printf 'ok\n'
    else
        printf 'FAILED (see %s.compile.log)\n' "$name"
        failed=$((failed + 1))
        continue
    fi

    if [ "$do_vm" -eq 1 ]; then
        if ! "$vmbuild" "$name" >"$name.build.log" 2>&1; then
            printf '  l1vm-build.sh FAILED (see %s.build.log)\n' "$name"
            failed=$((failed + 1))
            continue
        fi
        if [ "$do_run" -eq 1 ]; then
            printf '  running %s:\n' "$name"
            LD_LIBRARY_PATH="$modules${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
                "$vm" "$name" 2>&1 |
                grep -E '^(ok|FAIL)|done|ERROR|FATAL|memory_bounds|epos|stpopi'
        fi
    fi
done

printf '\n%s file(s) failed to compile/build\n' "$failed"
exit "$failed"
