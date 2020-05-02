#!/bin/sh

$CC -Wall -fPIC -g -c string.c -O3 -fomit-frame-pointer -g
$CC -shared -Wl,-soname,libl1vmstring.so.1 -o libl1vmstring.so.1.0 string.o
cp libl1vmstring.so.1.0 libl1vmstring.so
