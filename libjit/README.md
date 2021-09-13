L1VM jit-library README
=======================
In this directory is the L1VM JIT-compiler library.
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

addi, subi, muli, divi <br>
addd, subd, muld, divd <br>
andi, ori, bandi, bori, bxori <br>
eqi, neqi, gri, lsi, greqi, lseqi <br>
eqd, neqd, grd, lsd, greqd, lseqd <br><br>

jmp, jmpi <br><br>

movi, movd <br><br>


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

Windows
-------
In a shell do:

```
$ ./make-win.sh
```

Now you should have a *.so library file. Copy it to "usr/local/lib", or into the same directory
as the "l1vm" virtual machine.
