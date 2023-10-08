L1VM jit-library ARM AARCH64 README
===================================
In this directory is the L1VM JIT-compiler library for ARM ARRCH64.

NOTE THIS CODE DOESN'T WORK! The test program compiles the JIT-code.
But then the execution crashes it with a segmentation fault.
See the debug output "debug.txt"!

I need your help to fix this!

Also the asmjit library is needed: https://github.com/asmjit/asmjit

Run the script to clone and build "libasmjit":

```
$ ./install-libasmjit.sh
```

On Windows you have to do this steps by hand!
Or you run the L1VM in the Windows 10 WSL Linux system!

The library libl1vm-jit.so must be installed into "usr/local/lib" or
another library search path.

In the L1VM JIT-compiler version are the following opcodes included:

BUILD README
============
Linux
-----
In a shell use:

```
$ export CC=clang && export CCPP=clang++ && zerobuild force
```

Now you should have a *.so library file. Copy it to "usr/local/lib", or another
search path.
