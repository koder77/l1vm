#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c  main.c ../lib-func/file.c ../vm/modules/file/file-sandbox.c checkd.c ../lib-func/string.c ../lib-func/code_datasize.c
