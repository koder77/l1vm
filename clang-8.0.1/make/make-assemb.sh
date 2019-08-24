#!/bin/sh
~/l1vm-clang/bin/clang -Wall main.c ../lib-func/file.c checkd.c ../lib-func/string.c -o l1asm -g
