#!/bin/sh

$CCPP -Wall -fPIC -g -c mpfr-combined.cpp -O3 -fomit-frame-pointer -g
$CCPP -shared -Wl,-soname,libl1vmmpfr.so.1 -o libl1vmmpfr.so.1.0 mpfr-combined.o -lmpfr -lgmp
cp libl1vmmpfr.so.1.0 libl1vmmpfr.so
