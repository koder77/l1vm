#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c l1vm-optimize.c
