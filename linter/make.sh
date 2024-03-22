#!/bin/bash

if $CC -Wall -Wextra linter-main.c ../lib-func/file.c ../lib-func/string.c -o l1vm-linter -g; then
	exit 0
else
	exit 1
fi
