#!/bin/bash
if $CC -Wall -Wextra main.c ../lib-func/file.c checkd.c ../lib-func/string.c ../lib-func/code_datasize.c -o l1asm -g; then
	exit 0
	else
	exit 1
fi
