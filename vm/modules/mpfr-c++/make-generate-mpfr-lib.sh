#!/bin/sh

clang -Wall generate-mpfr-lib.c ../../../lib-func/file.c ../../../lib-func/string.c -o generate-mpfr-lib -g
