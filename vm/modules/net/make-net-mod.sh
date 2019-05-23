#!/bin/sh

clang -Wall -fPIC -g -c net.c ../../../string/string.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmnet.so.1 -o libl1vmnet.so.1.0 net.o
cp libl1vmnet.so.1.0 libl1vmnet.so
