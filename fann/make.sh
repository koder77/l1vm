#!/bin/bash
clang fann-train.c -o fann-train -lfann
clang fann-run.c -o fann-run -lfann
