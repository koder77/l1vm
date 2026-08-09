#!/bin/bash
clang fann-train.c -o fann-train -lfann -lm
clang fann-run.c -o fann-run -lfann -lm
