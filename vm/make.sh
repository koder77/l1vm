#!/bin/sh
clang++ -Wall jit.cpp -c -I/usr/local/include -O3 -fomit-frame-pointer
clang -Wall main.c load-object.c jit.o -o l1vm -ldl -lpthread -lasmjit -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -fomit-frame-pointer -g -Wl,--export-dynamic
#! without SDL library support:
#! clang main.c load-object.c jit.o -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
