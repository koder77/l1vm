#!/bin/bash
# set vm/main.c JIT_COMPILER to 0 and compile using this script
if clang -Wall main.c load-object.c debugger.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c -o l1vm-cli -lm -ldl -L../asmjit/build -lasmjit-l1vm -lpthread -O2 -g -march=native -fomit-frame-pointer -Wl,--export-dynamic; then
	exit 0
else
	exit 1
fi
