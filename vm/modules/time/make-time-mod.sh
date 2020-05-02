#!/bin/sh

$CC -Wall -fPIC -g -c time.c -O3 -fomit-frame-pointer -g
$CC -shared -Wl,-soname,libl1vmtime.so.1 -o libl1vmtime.so.1.0 time.o
cp libl1vmtime.so.1.0 libl1vmtime.so
