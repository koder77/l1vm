L1VM and C primes search
========================
BUILD
=====
$ ./build.sh
$ ./build-primes-c.sh

RUN
===
$ ./run-l1vm.sh
$ ./run-c.sh

The results on my machine:

$ ./run-c.sh

real	0m0,619s
user	0m0,132s
sys	0m0,487s

$ ./run-l1vm.sh

real	0m1,625s
user	0m1,608s
sys	0m0,016s

The L1VM is about factor 2.6 of the C program.
Without the JIT-compiler running!
Only the interpreter runs.
