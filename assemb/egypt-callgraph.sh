#!/bin/bash

rm *.o
gcc -Wall main.c ../lib-func/file.c checkd.c ../lib-func/string.c ../lib-func/code_datasize.c -fdump-rtl-expand -o l1asm -g;

egypt *.expand | dot -Grankdir=LR -Tps -o callgraph.ps
rm *.o
rm *.expand
rm l1asm
