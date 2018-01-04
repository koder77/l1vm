#!/bin/sh

clang-4.0 main.c load-object.c -o l1vm -ldl -lpthread -O3 -fomit-frame-pointer -funit-at-a-time -s -march=native -Wl,--export-dynamic
