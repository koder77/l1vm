#!/bin/sh
perf stat -e L1-icache-load-misses,L1-dcache-load-misses,branches,branch-misses luajit -e N=100000000  ~/l1vm/prog/primes-benchmark/primes.lua | column
