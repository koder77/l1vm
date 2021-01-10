L1VM jit-library DEMO README
============================
In this directory is the L1VM JIT-compiler demo library.
There are versions for Linux 64 bit (tested on Fedora 33) and Windows 10 WSL (Debian).
You need libcrypto installed on your system.
The demo version is limited to 40 opcodes which can be JIT-compiled!
You can get the full version in my ko-fi shop: https://ko-fi.com/koder77/shop

Also the asmjit library is needed: https://github.com/asmjit/asmjit

The library libl1vm-jit.so must be installed into /usr/local/lib or
another library search path.

In the L1VM JIT-compiler version are the following opodes included:

addi, subi, muli, divi <br>
addd, subd, muld, divd <br>
andi, ori, bandi, bori, bxori <br>
eqi, neqi, gri, lsi, greqi, lseqi <br>
eqd, neqd, grd, lsd, greqd, lseqd <br><br>

jmp, jmpi <br><br>

movi, movd <br><br>

Compile the VM with the "install-jit.sh" script.
