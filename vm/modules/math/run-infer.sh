#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c math.c mt19937-64.c
