#!/bin/sh

clang-3.9 -Wall -fPIC -g -c gpio.c -O3 -fomit-frame-pointer -g
clang-3.9 -shared -Wl,-soname,libl1vmgpio.so.1 -o libl1vmgpio.so.1.0 gpio.o -lm -lwiringPi
cp libl1vmgpio.so.1.0 libl1vmgpio.so
