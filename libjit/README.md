L1VM jit-library README
=======================
In this directory is the L1VM JIT-compiler library for X86-64.

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
$ ./make.sh
```

Now you should have a *.so library file. Copy it to "~/l1vm/bin".

Windows
-------
In a shell do:

```
$ ./make-win.sh
```

Now you should have a *.so library file. Copy it to "~/l1vm/bin", or into the same directory
as the "l1vm" virtual machine.
