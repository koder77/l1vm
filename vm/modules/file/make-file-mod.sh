#!/bin/sh

$CC -Wall -fPIC -g -c file.c file-sandbox.c ../../../lib-func/string.c -O3 -fomit-frame-pointer -g
$CC -shared -Wl,-soname,libl1vmfile.so.1 -o libl1vmfile.so.1.0 file.o file-sandbox.o string.o
cp libl1vmfile.so.1.0 libl1vmfile.so
