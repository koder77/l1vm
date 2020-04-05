#!/bin/sh

clang -Wall -fPIC -c -g fann-lib.c ../file/file-sandbox.c -O3
clang -Xlinker -L/usr/local/lib/ -v -shared -Wl,-soname,libl1vmfann.so.1 -o libl1vmfann.so.1.0 *.o -lm -lfann
cp libl1vmfann.so.1.0 libl1vmfann.so
