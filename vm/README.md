L1VM BUILD README  2024-05-28
=============================
BUILD AS EMBEDDED LIBRARY
-------------------------
You can set the "L1VM_EMBEDDED" flag to "1" and build the L1VM as a embedded library. And use this zerobuild files to build it:

<pre>
zerobuild-nojit-embedded.txt
zerobuild-nojit-embedded-macos.txt
zerobuild-nojit-embedded-win.txt
</pre>

An example caller program is in the "test-embedded" directory.
It just runs the "Hello world" program and dumps the global data memory.
You have to extract the data from it. You can use the data offsets as shown in the assembly output for this! So you can get the data out of the L1VM RAM.

Now there is a bash script to build L1VM without JIT-compiler: "make-nojit.sh" in vm directory. You have to set "JIT_COMPILER" to "0" in the source file vm/main.c to do that. In some cases programs execute faster if they don't need the JIT-compiler to run!

BUILD without JIT-compiler
--------------------------
<pre>
$ ./make-nojit.sh
</pre>

OR

<pre>
$ zerobuild zerobuild-nojit.txt force
</pre>

BUILD with JIT-compiler
-----------------------
Set "JIT_COMPILER" definition in vm/jit.h to "1"!
<pre>
#define JIT_COMPILER 1
</pre>

<pre>
$ ./make.sh
</pre>

OR

<pre>
$ zerobuild force
</pre>
