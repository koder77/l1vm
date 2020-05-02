#!/bin/sh

$CC -Wall -fPIC -g -c process.c -O3 -fomit-frame-pointer
$CC -shared -Wl,-soname,libl1vmprocess.so.1 -o libl1vmprocess.so.1.0 process.o
cp libl1vmprocess.so.1.0 libl1vmprocess.so
