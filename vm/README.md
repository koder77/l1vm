L1VM BUILD README  2019-06-20
=============================
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
Set "JIT_COMPILER" definition in vm/main.c to "1"!
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
