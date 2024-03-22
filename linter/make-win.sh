#!/bin/bash

clang -Wall -Wextra linter-main.c ../lib-func/file.c ../lib-func/string.c -o l1vm-linter -g -mwindows
