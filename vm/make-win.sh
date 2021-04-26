#!/bin/sh

clang -Wall -Wextra main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c -o l1vm -lpthread -Os -fomit-frame-pointer -funit-at-a-time -s -Wl,--export-all-symbols -mwindows -mconsole
