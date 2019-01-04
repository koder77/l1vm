#!/bin/sh

clang -Wall -fPIC -g -c sdl.c -O3 -fomit-frame-pointer -g0
clang -shared -Wl,-soname,libl1vmsdl.so.1 -o libl1vmsdl.so.1.0 sdl.o
cp libl1vmsdl.so.1.0 libl1vmsdl.so
