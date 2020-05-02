#!/bin/sh

$CC -Wall -fPIC -g -c math.c mt19937-64.c -O3 -fomit-frame-pointer -g
$CC -shared -Wl,-soname,libl1vmmath.so.1 -o libl1vmmath.so.1.0 math.o mt19937-64.o -lm
cp libl1vmmath.so.1.0 libl1vmmath.so
