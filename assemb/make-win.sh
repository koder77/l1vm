#!/bin/bash
clang -Wall -Wextra main.c ../lib-func/file.c ../vm/modules/file/file-sandbox.c checkd.c ../lib-func/string.c ../lib-func/code_datasize.c -o l1asm -g -mwindows -mconsole
