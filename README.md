L1VM README  2021-01-07
=======================
![alt text](https://midnight-koder.net/blog/assets/l1vm/L1VM-stern-3-300x424.png "L1VM logo")

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/2f0638b0ab6b433aad4d35c18d2f85c4)](https://www.codacy.com/app/koder77/l1vm?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=koder77/l1vm&amp;utm_campaign=Badge_Grade)

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P2Y3KP)

NEW: Now you can purchase the L1VM data base full version and the JIT-compiler at my new kofi shop: <br>
[kofi-shop-koder77](https://ko-fi.com/koder77/shop)

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

<h2>Credits</h2>
Thanks goes to: <br>
The team behind SDL and A. Schiffler (ferzkopp) for their great work! <br>
The author of genann networks library: Lewis Van Winkle (codeplea). <br>
The author of MPFR C++: Pavel Holoborodko. <br>
The author of the RS232 library I use: Teunis van Beelen. <br>
The authors of libserialport: the sigrok team. <br>
The authors of mt19937-64.c random number generator :Takuji Nishimura and Makoto Matsumoto. <br><br>

Without them this L1VM project would not be possible! Thank you! <br>
----------------------- <br>

<h2>New: l1pre - the preprocessor</h2>
The new "l1pre" preprocessor can be used to define macros and include files.
See the new "include-lib" directory with all header files. The "prog/hello-6.l1com" example shows how to use this!

<h2>New SDL 2.0!</h2>
Finally I ported the SDL gfx/GUI library to SDL 2.0!
The source code is in: vm/modules/sdl-2.0/

I did remove the SDL 1.2 code from this repo. So for now the SDL 2.0 libraries are needed
to build my L1VM SDL library.

You need to build vm-sdl2/ VM sources to use the new SDL 2.0 gfx/GUI library.
This source of the L1VM is linked with the SDL2 libraries.


<h2>1. Configuration</h2>
Configure the file access for SANDBOX mode or not, and set your /home directory name:
include/global.h:

<pre>
// SANDBOX FILE ACCESS
#define SANDBOX                 1			// secure file acces: 1 = use secure access, 0 = OFF!!
#define SANDBOX_ROOT			"/l1vm/"	// in /home directory!
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

<h3>Build with JIT-compiler</h3>
Edit the "vm/jit.h" file, set:

<pre>
#define JIT_COMPILER 1
</pre>


<h2>2. Installation</h2>
<h3>Debian Linux</h3>
Use: install-debian.sh or install-jit-debian.sh

<h3>Fedora Linux</h3>
Use: install-zerobuild-fedora.sh or install-jit-zerobuild-fedora.sh

<h3>Windows 10 WSL</h3>
Use: install-wsl-debian.sh or install-wsl-debian-jit.sh

Make the bash install script executable by:

<pre>
$ chmod +x ./install-wsl-debian.sh
</pre>

And run it:
<pre>
$ ./install-wsl-debian.sh
</pre>

Note: the install scripts automatically install clang C compiler and my zerobuild build tool!

<h3>Windows Cygwin</h3>
You have to install the clang compiler, SDL2 libraries and a window manager like Xfce for graphics output.
This can be done with the installer on: https://www.cygwin.com/

Edit the file: "vm/jit.h": and set:
<pre>
#define JIT_COMPILER 0
</pre>

The build script is: "install-zerobuild-cygwin.sh".
Note: at the moment only this modules can be  build: endianess, file, genann, math, mem, process, sdl, time.


You need the "zerobuild" tool to build the VM: https://www.github.com/koder77/zerobuild

And run the "install-jit-zerobuild.sh" script.

<h3>Build without JIT-compiler</h3>
Set "JIT_COMPILER" to "0"

And run the "install-zerobuild.sh" script.

<h3>cli only</h3>

To compile for cli (bashs) text in/output only with no SDL gfx support:
Set "#define SDL_module 0" in "include/global.h".
Then run in "vm/" the bash script: "make-cli.sh".

<h3>DragonFly BSD</h3>
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

<h2>Modules</h2>
<pre>
Cells - linked neural networks with FANN library
endianess - convert to big endian, or little endian functions
fann - FANN neural networks
file - file module
genann - neural networks module
gpio - Raspberry Pi GPIO module
math - some math functions
mem - memory allocating for arrays and vectors
mem-vect - C++ vector memory library
mpfr-c++ - MPFR floating point big num library
net - TCP/IP sockets module
process - start a new shell process
rs232-libserialport - RS232 serial port using libserialport
rs232 - RS232 serial port module
sdl 2.0 - graphics primitves module, like pixels, lines..., and GUI with buttons, lists, etc.
string - some string functions
time - get time and date
</pre>

<h2>NEW</h2>
MPFR floating point big numbers library added!
Now with the MPFR library calculations with high precision can be done.
See the example in the lib/ directory: "mpfr-lib-auto.l1com".
There are about 87 math functions in the MPFR library.

<h2>NOTE</h2>
The current version of L1VM only runs on a Linux or other POSIX compatible OS!
It works on Cygwin and on Windows 10 WSL too!
If you want help to port it to a new OS, then contact me please...

<h2>TODO</h2>
	- make the L1COM compiler a bit more comfortable
	- write more functions for the modules
	- more demo programs

<h2>USAGE</h2>
<h3>compile</h3>
<pre>
$ l1com test
</pre>
compiles program "test.l1com"

<h3>run</h3>
<pre>
$ l1vm test
</pre>
finally executes program "test.l1obj"
