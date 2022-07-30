#!/bin/bash

clang -Wall -Wextra main.c labels.c register.c var.c parse-rpolish.c ../lib-func/file.c checkd.c if.c mem.c ../lib-func/string.c -o l1com -g -mwindows
