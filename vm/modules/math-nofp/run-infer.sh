#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c fp16.c math-nofp.c mt19937-64.c
