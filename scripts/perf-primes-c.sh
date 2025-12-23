#!/bin/sh
perf stat -e L1-icache-load-misses,L1-dcache-load-misses,branches,branch-misses ~/l1vm/prog/primes-benchmark/primes
