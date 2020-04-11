#!/bin/sh

if clang -Wall main.c labels.c register.c var.c ../lib-func/file.c checkd.c if.c mem.c ../lib-func/string.c -o l1com -g; then
	exit 0
else
	exit 1
fi
