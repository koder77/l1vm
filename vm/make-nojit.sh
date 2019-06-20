#!/bin/sh
clang main.c load-object.c ../lib-func/string.c -o l1vm-nojit -ldl -lpthread -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -fomit-frame-pointer -g -Wl,--export-dynamic
#! without SDL library support:
#! clang main.c load-object.c ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
