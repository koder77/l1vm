#!/bin/sh

$CC -Wall -fPIC -g -c main.c ../../../lib-func/string.c -O3
$CC -shared -Wl,-soname,libl1vmrs232.so.1 -o libl1vmrs232.so.1.0 main.o string.o -I/usr/local/include -L/usr/local/lib -lserialport
cp libl1vmrs232.so.1.0 libl1vmrs232.so
