#!/bin/sh

$CC -Wall -fPIC -g -c main.c rs232.c ../../../lib-func/string.c -O3
$CC -shared -Wl,-soname,libl1vmrs232.so.1 -o libl1vmrs232.so.1.0 main.o rs232.o string.o
cp libl1vmrs232.so.1.0 libl1vmrs232.so
