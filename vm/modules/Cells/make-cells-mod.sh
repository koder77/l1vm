#!/bin/sh

clang -Wall -fPIC -c -g Cells-lib.c -O3
clang -Xlinker -L/usr/local/lib/ -v -shared -Wl,-soname,libl1vmcells.so.1 -o libl1vmcells.so.1.0 *.o -lm -lfann -lcells
cp libl1vmcells.so.1.0 libl1vmcells.so
