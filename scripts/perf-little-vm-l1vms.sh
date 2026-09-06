#!/bin/sh
perf stat -e L1-icache-load-misses,L1-dcache-load-misses,branches,branch-misses l1vms little-vm -q -C 1 -q
