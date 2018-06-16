#!/bin/sh
#! -funit-at-a-time
clang-4.0 main.c load-object.c -o l1vm -ldl -lpthread -Os -fomit-frame-pointer -s -Wl,--export-dynamic
