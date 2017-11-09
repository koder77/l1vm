#!/bin/sh

clang -Wall -fPIC -g -c math.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmmath.so.1 -o libl1vmmath.so.1.0 math.o -lm
cp libl1vmmath.so.1.0 libl1vmmath.so
