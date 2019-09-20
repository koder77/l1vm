#!/bin/sh
# set vm/main.c JIT_COMPILER to 0 and compile using this script
~/l1vm-clang-9.0.0/bin/clang -Wall main.c load-object.c ../lib-func/string.c -o l1vm-nojit -ldl -lpthread -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -fomit-frame-pointer -Wl,--export-dynamic
# without SDL library support:
# clang -Wall main.c load-object.c ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
