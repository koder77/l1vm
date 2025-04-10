#!/bin/bash -e
# get infer linter from: https://fbinfer.com/

infer run -- clang -c gui.c sdl.c string.c ../file/file-sandbox.c
