#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c l1vmform.c ../lib-func/file.c ../lib-func/string.c
