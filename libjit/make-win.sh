#!/bin/sh
rm *.o
rm *.cpo
rm *.so
clang++ -I../include  -I/usr/local/include -O2 -fomit-frame-pointer -Wall -c -o jit.cpo jit.cpp
clang -o libl1vm-jit.so jit.cpo  -L/usr/local/lib -shared -lasmjit -lstdc++
