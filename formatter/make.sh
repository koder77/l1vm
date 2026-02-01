#!/bin/bash

if $CC -Wall -Wextra l1vmform .c ../lib-func/file.c ../lib-func/string.c -o l1vm-form -g; then
	exit 0
else
	exit 1
fi
