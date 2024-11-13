#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c main.c load-object.c ../lib-func/string.c ../lib-func/code_datasize.c ../lib-func/memory_bounds.c ../libjit/jit.cpp
