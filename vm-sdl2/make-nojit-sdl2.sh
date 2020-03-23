#!/bin/sh
# set vm/main.c JIT_COMPILER to 0 and compile using this script
if clang -Wall main.c load-object.c ../lib-func/string.c -o l1vm-nojit -lm -ldl -lpthread -lSDL2 -lSDL2_gfx -lSDL2_image -lSDL2_ttf -O2 -g -fomit-frame-pointer -I/usr/include/SDL -Wl,--export-dynamic; then
	exit 0
else
	exit 1
fi
# without SDL library support:
# clang -Wall main.c load-object.c ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
