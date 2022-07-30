#!/bin/bash

if $CC -Wall -Wextra main.c ../lib-func/file.c ../lib-func/string.c -o l1pre -g; then
	exit 0
else
	exit 1
fi
