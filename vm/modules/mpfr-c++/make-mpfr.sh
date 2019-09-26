#!/bin/sh

clang++ -Wall -fPIC -g -c mpfr.cpp -O3 -fomit-frame-pointer -g
clang++ -shared -Wl,-soname,libl1vmmpfr.so.1 -o libl1vmmpfr.so.1.0 mpfr.o -lmpfr -lgmp
cp libl1vmmpfr.so.1.0 libl1vmmpfr.so
