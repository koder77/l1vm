#!/bin/sh
# set vm/main.c JIT_COMPILER to 0 and compile using this script
if clang -Wall main.c load-object.c ../lib-func/string.c -o l1vm-cli -lm -ldl -lpthread -O2 -g -fomit-frame-pointer -Wl,--export-dynamic; then
	exit 0
else
	exit 1
fi
