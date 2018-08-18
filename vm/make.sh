#!/bin/sh
#! -funit-at-a-time
clang++ jit.cpp -c -I/usr/local/include -O3 -fomit-frame-pointer
clang main.c load-object.c jit.o -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
