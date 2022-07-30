#!/bin/bash

rm *.o
gcc -Wall main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c -o l1vm-nojit -lm -ldl -lpthread -lstdc++ -lSDL2 -lSDL2_gfx -lSDL2_image -lSDL2_ttf -Os -g -fdump-rtl-expand -fomit-frame-pointer -Wl,--export-dynamic

egypt *.expand | dot -Grankdir=LR -Tps -o callgraph.ps
rm *.o
rm *.expand
rm l1vm-nojit
