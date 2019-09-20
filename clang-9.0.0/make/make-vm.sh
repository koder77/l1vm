#!/bin/sh
~/l1vm-clang-9.0.0/bin/clang++ -Wall jit.cpp -c -I/usr/local/include -O3 -fomit-frame-pointer
~/l1vm-clang-9.0.0/bin/clang -Wall main.c load-object.c jit.o ../lib-func/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -fomit-frame-pointer -g -Wl,--export-dynamic
#! without SDL library support:
#! clang main.c load-object.c jit.o ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
