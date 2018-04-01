#!/bin/sh

clang-3.9 -Wall -fPIC -c -g genann.c -O3
clang-3.9 -Wall -fPIC -c -g genann-lib.c -O3
clang-3.9 -Xlinker -L/usr/local/lib/ -v -shared -Wl,-soname,libl1vmgenann.so.1 -o libl1vmgenann.so.1.0 *.o -lm
cp libl1vmgenann.so.1.0 libl1vmgenann.so
