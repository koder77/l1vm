#!/bin/sh

clang -Wall -fPIC -g -c endianess.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmfile.so.1 -o libl1vmendianess.so.1.0 endianess.o
cp libl1vmendianess.so.1.0 libl1vmendianess.so
