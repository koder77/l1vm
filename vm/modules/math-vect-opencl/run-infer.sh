#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c math-vect-double-opencl.c math-vect-int-opencl.c
