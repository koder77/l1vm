#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c brackets-code.c dsl.c embed.c learn.c
