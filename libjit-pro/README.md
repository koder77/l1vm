L1VM jit-library PRO README
===========================
In this directory is the L1VM jit PRO library, as *.so binary file for Linux.

Also the asmjit library is needed: https://github.com/asmjit/asmjit

The library libl1vm-jit.so must be installed into /usr/local/lib or
another library search path.

In the L1VM JIT-compiler PRO version are the following opodes included:

addi, subi, muli, divi <br>
addd, subd, muld, divd <br>
bandi, bori, bxori <br>
eqi, neqi, gri, lsi, greqi, lseqi <br>
eqd, neqd, grd, greqd, lseqd <br><br>

jmp, jmpi <br><br>

movi, movd <br><br>

Set THE JIT-compiler type TO PRO in the "include/global.h" file:
#define JIT_COMPILER_PRO 1

And compile the VM with the "install-jit.sh" script.

Write me a e-mail with the subject "L1VM PRO license" and I will send you a license keyfile for FREE! <br>
spietzonke _at_ gmail _dot_ net
