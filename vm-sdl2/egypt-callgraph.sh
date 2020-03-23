#!/bin/sh

rm *.o
gcc -Wall main.c load-object.c ../lib-func/string.c -o l1vm-nojit -lm -ldl -lpthread -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -g -fdump-rtl-expand -fomit-frame-pointer -Wl,--export-dynamic

egypt *.expand | dot -Grankdir=LR -Tps -o callgraph.ps
rm *.o
rm *.expand
rm l1vm-nojit
