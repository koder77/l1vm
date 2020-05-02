#!/bin/sh

$CC -Wall -fPIC -c -g Cells-lib.c ../file/file-sandbox.c -O3
$CC -Xlinker -L/usr/local/lib/ -v -shared -Wl,-soname,libl1vmcells.so.1 -o libl1vmcells.so.1.0 *.o -lm -lfann -lcells
cp libl1vmcells.so.1.0 libl1vmcells.so
