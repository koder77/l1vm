#!/bin/sh
# set vm/main.c JIT_COMPILER to 0 and compile using this script
if clang -Wall main.c load-object.c ../lib-func/string.c -o l1vm-nojit -lm -ldl -lpthread -lstdc++ -lSDL -lSDL_gfx -lSDL_image -lSDL_ttf -Os -g -fomit-frame-pointer -Wl,--export-dynamic; then
	exit 0
else
	exit 1
fi
# without SDL library support:
# clang -Wall main.c load-object.c ../string/string.c -o l1vm -ldl -lpthread -lasmjit -lstdc++ -Os -fomit-frame-pointer -g -Wl,--export-dynamic
