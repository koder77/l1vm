#!/bin/sh

rm *.o
gcc -Wall main.c ../lib-func/file.c checkd.c ../lib-func/string.c -fdump-rtl-expand -o l1asm -g;

egypt *.expand | dot -Grankdir=LR -Tps -o callgraph.ps
rm *.o
rm *.expand
rm l1asm
