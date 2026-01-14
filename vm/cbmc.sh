#!/bin/sh
cbmc main.c load-object.c debugger.c ../lib-func/string.c \
--pointer-check --bounds-check --signed-overflow-check \
  --pointer-overflow-check --div-by-zero-check \
  --unwind 2 --object-bits 16 --slice-formula \
  --no-unwinding-assertions > cbmc.txt 2>&1; grep -E "failed|passed|SUCCESS|FAILURE" cbmc.txt | tail -n 20
