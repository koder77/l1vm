#!/bin/bash
CC=${CC:-cc}
if $CC -Wall -Wextra -I../include l1vm-optimize.c -o l1vm-optimize -g; then
	exit 0
else
	exit 1
fi
