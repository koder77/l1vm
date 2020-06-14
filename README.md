L1VM README  2020-06-07
=======================
![alt text](https://midnight-koder.net/blog/assets/l1vm/L1VM-stern-3-300x424.png "L1VM logo")

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/2f0638b0ab6b433aad4d35c18d2f85c4)](https://www.codacy.com/app/koder77/l1vm?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=koder77/l1vm&amp;utm_campaign=Badge_Grade)

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P2Y3KP)

L1VM is an incredible tiny virtual machine with RISC (or comparable style) CPU, about 61 opcodes and about 40 KB binary size on X86_64 Linux (without JIT-compiler)!
The VM has a 64 bit core (256 registers for integer and double float) and can run object code
written in brackets (a high level programming language) or l1asm assembly language.

Code and data are in separated memories for a secure execution. Like in Harvard type CPUs (found now in DSPs or microcontrollers).
The opcode set with 61 opcodes is my own opinion how things should work. It does not "copy" other instruction sets known in
other "real" CPUs.

The design goals are:
<pre>
	- be small
	- be fast
	- be simple
	- be modular
</pre>

In pure console text I/O programs not linked with SDL library, the memory footprint is very low.
About 2 MB RAM only as shown in the taskmanager by running a minimal test program!

New
---
L1VM JIT-Compiler PRO version, see libjit-pro folder for more info.
The PRO version has more opcodes as the open source version.
You can upgrade to PRO, if you write me an email.
See "libjit-pro" folder here!

New SDL 2.0!
------------
Finally I ported the SDL gfx/GUI library to SDL 2.0!
The source code is in: vm/modules/sdl-2.0/

I did remove the SDL 1.2 code from this repo. So for now the SDL 2.0 libraries are needed
to build my L1VM SDL library.

You need to build vm-sdl2/ VM sources to use the new SDL 2.0 gfx/GUI library.
This source of the L1VM is linked with the SDL2 libraries.


Installation
------------
You need the "zerobuild" tool to build the VM: https://www.github.com/koder77/zerobuild

Configure the file access for SANDBOX mode or not, and set your /home directory name:
include/global.h:

<pre>
// SANDBOX FILE ACCESS
#define SANDBOX                 1			// secure file acces: 1 = use secure access, 0 = OFF!!
#define SANDBOX_ROOT			"/home/stefan/l1vm/"		// in /home directory!
</pre>

And you can choose if the array variable bounds check and math checks should be run:

<pre>
// data bounds check exactly
#define BOUNDSCHECK				1
</pre>

<pre>
// integer and double floating point math functions overflow detection
// See vm/main.c interrupt0:
// 251 sets overflow flag for double floating point number
// 252 returns value of overflow flag
#define MATH_LIMITS				0

// set if only double numbers calculation results are checked (0), or
// arguments and results get checked for full check (1)
#define MATH_LIMITS_DOUBLE_FULL	0
</pre>

Build with JIT-compiler
-----------------------
Edit the "vm/jit.h" file, set:

<pre>
#define JIT_COMPILER 1
</pre>

And run the "install-jit-zerobuild.sh" script.

Build without JIT-compiler
--------------------------
Set "JIT_COMPILER" to "0"

And run the "install-zerobuild.sh" script.


Windows 10 WSL
--------------
Make the bash install script executable by:

<pre>
$ chmod +x ./install-wsl-debian.sh
</pre>

And run it:
<pre>
$ ./install-wsl-debian.sh
</pre>


cli only
--------
To compile for cli (bashs) text in/output only with no SDL gfx support:
Set "#define SDL_module 0" in "include/global.h".
Then run in "vm/" the bash script: "make-cli.sh".

DragonFly BSD
-------------
You have to add a "bin/" directory in your "/home/user" directory:

<pre>
$ mkdir bin
</pre>

Then you need to paste the following lines to your ".shrc" bash config
in your "/home" directory:

<pre>
# check if local bin folder exist
# $HOME/bin
# prepend it to $PATH if so
if [ -d $HOME/bin ]; then
    export PATH=$HOME/bin:$PATH
fi
</pre>


And finally run the installation script:
<pre>
$ sh ./install-DragonFly.sh
</pre>

That's it! Note the rs232 library is not build yet!

Now there is a bash script to build L1VM without JIT-compiler: "make-nojit.sh" in vm directory. You have to set "JIT_COMPILER" to "0" in the source file vm/main.c to do that. In some cases programs execute faster if they don't need the JIT-compiler to run!

I added a JIT-compiler using asmjit library. At the moment only few opcodes can be translated into code for direct execution.

L1VM ist under active development. As a proof of concept I rewrote the Nano VM fractalix SDL graphics demo in L1VM
assembly.

L1VM is 6 - 7 times faster than Nano VM, this comes from the much simpler design and dispatch speedup.

I included a few demo programs.

The source code is released under the GPL.

A simple "Hello world!" in bra(ets (brackets) my language for L1VM:

<pre>
// hello.l1com
(main func)
	(set int64 1 zero 0)
	(set string 13 hello "Hello world!")
	// print string
	(6 hello 0 0 intr0)
	// print newline
	(7 0 0 0 intr0)
	(255 zero 0 0 intr0)
(funcend)
</pre>

Modules
-------
<pre>
Cells - linked neural networks with FANN library
endianess - convert to big endian, or little endian functions
fann - FANN neural networks
file - file module
genann - neural networks module
gpio - Raspberry Pi GPIO module
math - some math functions
mpfr-c++ - MPFR floating point big num library
net - TCP/IP sockets module
process - start a new shell process
rs232-libserialport - RS232 serial port using libserialport
rs232 - RS232 serial port module
sdl 2.0 - graphics primitves module, like pixels, lines..., and GUI with buttons, lists, etc.
string - some string functions
time - get time and date
</pre>

NEW
---
MPFR floating point big numbers library added!
Now with the MPFR library calculations with high precision can be done.
See the example in the lib/ directory: "mpfr-lib-auto.l1com".
There are about 87 math functions in the MPFR library.

NOTE
----
The current version of L1VM only runs on a Linux or other POSIX compatible OS!
If you want help to port it to a new OS, then contact me please...

TODO
----
	- make the L1COM compiler a bit more comfortable
	- write more functions for the modules
	- more demo programs

USAGE
-----

compile
-------
<pre>
$ l1com test
</pre>
compiles program "test.l1com"

assemble
--------
<pre>
$ l1asm test
</pre>
assembles program "test.l1asm" generated by the compiler

run
---
<pre>
$ l1vm test
</pre>
finally executes program "test.l1obj"

==========================================================================
