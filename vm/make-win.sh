#!/bin/bash

clang -Wall -Wextra main.c load-object.c debugger.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c -o l1vm -lpthread -Os -fomit-frame-pointer -s -Wl,--export-all-symbols -mconsole
