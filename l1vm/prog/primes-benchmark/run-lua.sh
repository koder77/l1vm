#!/bin/sh
lua -e N=100000000 primes.lua | column
