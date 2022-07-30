#!/bin/bash
# set vm/main.c JIT_COMPILER to 0 and compile using this script
if $CC -Wall -Wextra main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c -o l1vm-nojit -lm -ldl -lpthread -O2 -g -fomit-frame-pointer -I/usr/include/SDL -Wl,--export-dynamic; then
	exit 0
else
	exit 1
fi
# without SDL library support:
# clang -Wall main.c load-object.c ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
