#!/bin/sh

clang-3.9 main.c load-object.c -o l1vm -ldl -lpthread -Os -fomit-frame-pointer -funit-at-a-time -s -Wl,--export-dynamic
