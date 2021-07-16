L1VM interrupts table
=====================

INTR0
-----
0: load module
1: free module
2: set module function
3: call module function
4: print integer
5: print double float
6: print string
7: print newline
8: delay
9: input integer
10: input double float
11: input string
12: shell arguments number
13: get shell argument
14: show stack pointer
15: return number of CPU cores
16: return endianess of host: little endian = 0, big endian = 1
17: return current time: hh mm ss
18: return current date: year month day
19: return weekday since sunday: 0 - 6
20: print integer number with format string
21: print double number with format string setup
22: print int16 integer number
23: print int32 integer number
24: start timer
25: stop timer
26: stack check: exit program if there is something on the stack, while it should not be there!!
27: print current epos and "INTR0: 27 DEBUG marking"
251: check double number overflow
252: get overflow flag
253: run JIT-compiler
254: run JIT-code
255: EXIT program


INTR1
-----
0: run new CPU
1: join threads
2: lock data mutex
3: unlock data mutex
4: return number of current CPU core
5: return number of free CPU cores
255: thread EXIT
