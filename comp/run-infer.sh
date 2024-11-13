#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c  main.c labels.c register.c var.c parse-rpolish.c ../lib-func/file.c checkd.c if.c mem.c ../lib-func/string.c
