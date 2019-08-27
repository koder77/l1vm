#!/bin/sh

clang main.c ../lib-func/file.c checkd.c if.c mem.c ../lib-func/string.c -o l1com -g -mwindows
