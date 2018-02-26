#!/bin/sh

gcc -Wall -fPIC -g -c net.c -O3 -fomit-frame-pointer -g
gcc -shared -Wl,-soname,libl1vmnet.so.1 -o libl1vmnet.so.1.0 net.o
cp libl1vmnet.so.1.0 libl1vmnet.so
