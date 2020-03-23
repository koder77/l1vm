L1VM BUILD README  2020-03-23
=============================
NEW: This is a experimental version of my L1VM using the SDL 2 libraries.
The SDL 2 library is in the vm/modules/sdl-2.0/ directory.

Now only the gfx primitives like pixels and line drawing works.
And the GUI is not working at the moment!!! I have to investigate what is wrong right now...

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
