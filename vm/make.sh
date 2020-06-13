#!/bin/sh
# $CCPP -Wall jit.cpp -c -I/usr/local/include -O3 -fomit-frame-pointer
#clang -Wall main.c load-object.c jit.o ../lib-func/string.c -o l1vm -lm -ldl -lpthread -lasmjit -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -fomit-frame-pointer -g -Wl,--export-dynamic

$CC -Wall main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/file.c -o l1vm -L/usr/local/lib -lstdc++ -lm -ldl -lpthread -lasmjit -ll1vm-jit -lSDL2 -lSDL2_gfx -lSDL2_image -lSDL2_ttf -Os -fomit-frame-pointer -g -Wl,--export-dynamic
#! without SDL library support:
#! clang main.c load-object.c jit.o ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
