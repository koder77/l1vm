#!/bin/sh

$CC -Wall -fPIC -g -c mem.c -O3 -fomit-frame-pointer -g
$CC -shared -Wl,-soname,libl1vmmem.so.1 -o libl1vmmem.so.1.0 mem.o -lm
cp libl1vmmem.so.1.0 libl1vmmem.so
