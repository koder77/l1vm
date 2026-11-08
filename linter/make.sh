#!/bin/bash

# The Z3 solver is optional. When the development files are installed the
# linter gets the bounded model checker pass, otherwise it compiles as before.
z3_cflags=""
z3_libs=""
z3_define=""

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists z3; then
	z3_define="-DHAVE_Z3"
	z3_cflags="$(pkg-config --cflags z3)"
	z3_libs="$(pkg-config --libs z3)"
elif [ -f /usr/include/z3.h ] || [ -f /usr/local/include/z3.h ]; then
	z3_define="-DHAVE_Z3"
	z3_libs="-lz3"
fi

if $CC -Wall -Wextra $z3_cflags $z3_define linter-main.c linter-solver.c ../lib-func/file.c ../lib-func/string.c -o l1vm-linter -g $z3_libs; then
	exit 0
else
	exit 1
fi
