#!/bin/sh

clang-3.9 -Wall -fPIC -g -c main.c rs232.c -O3
clang-3.9 -shared -Wl,-soname,libl1vmrs232.so.1 -o libl1vmrs232.so.1.0 main.o rs232.o
cp libl1vmrs232.so.1.0 libl1vmrs232.so
