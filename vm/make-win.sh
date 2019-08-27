#!/bin/sh

clang main.c load-object.c -o l1vm -lpthread -Os -fomit-frame-pointer -funit-at-a-time -s -Wl,--export-all-symbols -mwindows -mconsole
