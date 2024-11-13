#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c nanoid.c
