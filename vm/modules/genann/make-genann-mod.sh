#!/bin/sh

clang -Wall -fPIC -c -g genann.c -O3
clang -Wall -fPIC -c -g genann-lib.c -O3
clang -Xlinker -L/usr/local/lib/ -v -shared -Wl,-soname,libl1vmgenann.so.1 -o libl1vmgenann.so.1.0 *.o -lm
cp libl1vmgenann.so.1.0 libl1vmgenann.so
