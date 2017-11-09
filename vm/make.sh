#!/bin/sh

clang main.c load-object.c -o l1vm -ldl -lpthread -O3 -fomit-frame-pointer -funit-at-a-time -march=native -g -Wl,--export-dynamic
