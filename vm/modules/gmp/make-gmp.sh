#!/bin/sh

clang -Wall -fPIC -g -c gmp.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmgmp.so.1 -o libl1vmgmp.so.1.0 gmp.o -lm -lgmp
cp libl1vmgmp.so.1.0 libl1vmgmp.so
