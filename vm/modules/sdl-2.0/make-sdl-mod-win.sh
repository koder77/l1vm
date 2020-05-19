#!/bin/sh

gcc -Wall -fPIC -g -c sdl.c -O3 -fomit-frame-pointer -g0 -s -I/usr/include/SDL
gcc -shared -Wl,-soname,libl1vmsdl.so.1 -o libl1vmsdl.so.1.0 sdl.o -L/usr/lib -lSDL -lSDL_gfx -lSDL_ttf
cp libl1vmsdl.so.1.0 libl1vmsdl.so
