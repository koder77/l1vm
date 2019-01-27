#!/bin/sh

clang -Wall -fPIC -g -c sdl.c gui.c string.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmsdl.so.1 -o libl1vmsdl.so.1.0 sdl.o gui.o string.o
cp libl1vmsdl.so.1.0 libl1vmsdl.so
