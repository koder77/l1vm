#!/bin/bash

clang -Wall -Wextra l1vmform.c ../lib-func/file.c ../lib-func/string.c -o l1vm-form -g -mwindows
