#!/bin/sh

clang -Wall -fPIC -g -c math.c mt19937ar.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmmath.so.1 -o libl1vmmath.so.1.0 math.o mt19937ar.o -lm
cp libl1vmmath.so.1.0 libl1vmmath.so
