#!/bin/sh

clang -Wall -fPIC -g -c gpio.c -O3 -fomit-frame-pointer -g
clang -shared -Wl,-soname,libl1vmgpio.so.1 -o libl1vmgpio.so.1.0 gpio.o -lm -lwiringPi
cp libl1vmgpio.so.1.0 libl1vmgpio.so
