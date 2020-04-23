#!/bin/sh

clang -Wall -fPIC -g -c net.c ../../../lib-func/string.c ../file/file-sandbox.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmnet.so.1 -o libl1vmnet.so.1.0 net.o string.o file-sandbox.o
cp libl1vmnet.so.1.0 libl1vmnet.so
