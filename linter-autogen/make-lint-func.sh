#!/bin/sh

if [ "$#" -lt 1 ]
then
    echo "make-lint-func.sh <C-function> <C-function-out> <module-header> <linter-include-out>"
    exit 1
fi

./l1vm-cfunc $1 $2
./l1vm-func $3 $2 $4
