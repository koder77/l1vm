L1VM jit-library README
=======================
In this directory is the L1VM JIT-compiler library.
Also the asmjit library is needed: https://github.com/asmjit/asmjit

The library libl1vm-jit.so must be installed into /usr/local/lib or
another library search path.

In the L1VM JIT-compiler version are the following opodes included:

addi, subi, muli, divi <br>
addd, subd, muld, divd <br>
bandi, bori, bxori <br>
eqi, neqi, gri, lsi, greqi, lseqi <br>
eqd, neqd, grd, lsd, greqd, lseqd <br><br>

jmp, jmpi <br><br>

movi, movd <br><br>

Compile the VM with the "install-jit.sh" script.
