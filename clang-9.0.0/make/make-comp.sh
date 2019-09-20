#!/bin/sh

~/l1vm-clang-9.0.0/bin/clang -Wall main.c ../lib-func/file.c checkd.c if.c mem.c ../lib-func/string.c -o l1com -g
