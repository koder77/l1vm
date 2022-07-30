#!/bin/bash

clang -Wall -Wextra main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c -o l1vm -lpthread -Os -fomit-frame-pointer -funit-at-a-time -s -Wl,--export-all-symbols -mwindows -mconsole
