#!/bin/sh

rm *.o
gcc -Wall main.c load-object.c ../lib-func/string.c -o l1vm-nojit -lm -ldl -lpthread -lstdc++ -lSDL2 -lSDL2_gfx -lSDL2_image -lSDL2_ttf -Os -g -fdump-rtl-expand -fomit-frame-pointer -Wl,--export-dynamic

egypt *.expand | dot -Grankdir=LR -Tps -o callgraph.ps
rm *.o
rm *.expand
rm l1vm-nojit
