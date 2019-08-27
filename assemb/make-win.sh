#!/bin/sh
clang main.c ../lib-func/file.c checkd.c ../lib-func/string.c -o l1asm -g -mwindows -mconsole
