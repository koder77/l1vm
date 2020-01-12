#!/bin/sh
if clang -Wall main.c ../lib-func/file.c checkd.c ../lib-func/string.c -o l1asm -g; then
	exit 0
	else
	exit 1
fi
