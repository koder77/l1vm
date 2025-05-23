L1VM README  2025-05-23
=======================
![alt text](https://midnight-coding.de/blog/assets/l1vm/L1VM-stern-3-300x424.png "L1VM logo")

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P2Y3KP)

Running my blog and keep my hardware going costs money! So you can support me on Ko-fi!

[![C/C++ CI](https://github.com/koder77/l1vm/actions/workflows/github-ci.yaml/badge.svg)](https://github.com/koder77/l1vm/actions/workflows/github-ci.yaml)

The Brackets language is a modern, general-purpose programming language. The focus is on reliability and speed. <br>
The language can be used for CLI and also full GUI programs. Use multiple CPU cores with multithreading. <br>
The Brackets bytecode can run on any supported OS without changes. This makes it truly platform independent. <br><br>

<b>Computing at a scale. Use the <a href="https://github.com/advanpix/mpreal">MPFR</a> module for arbitrary number precision math. Scale from SBCs to desktop PCs and beyond. 
Powerful modules are available at your fingertips. You can develop your own modules to fit your needs. </b> <br><br>

The L1VM is available for: Linux (x86_64, AARCH64), OpenBSD, FreeBSD, DragonFly BSD, Windows (10, 11) via WSL or native MSYS2, macOS and Haiku OS. <br>
On the Raspberry Pi the GPIO pins and the serial port can be used via the modules. <br><br>

Use network functions with TCP/IP and <a href="https://midnight-coding.de/blog/software/l1vm/2024/10/09/L1VM-net-module-ssl.html">TLS/SSL sockets</a>. <br><br>

<b>The L1VM is memory safe. Array overflows and string overflows are catched at runtime. The execution will break on an error. <br>
There can be legal limits set for a number variable. Resulting in a runtime error on overflow. </b> <br>
The VM has no garbage collector. All variables declared by "set" are in memory at program start. No memory management must be done on them. <br><br>

Here is the [L1VM course](https://midnight-coding.de/blog/l1vm) on my blog.

The L1VM can be build as a shared library to embedd it too!
</b>

<h2>Build the L1VM as a shared library</h2>
You need to make a change in "include/settings.h": Set the EMBEDDED flag to "1":

<pre>
#define L1VM_EMBEDDED 1
</pre>

Then build the library in "vm/" with one of the zerobuild embedded build scripts. On Linux you need this for example:

<pre>
$ CC=clang CCPP=clang++ zerobuild zerobuild-nojit-embedded.txt force
</pre>

Now copy the ".so" file to the  "~/l1vm/bin" directory. So it can be found by the system.

As a next step you can build the test program in "vm/test-embedded".
To build the example:
<pre>
$ CC=clang CCPP=clang++ zerobuild force
</pre>

To run the test program:

<pre>
$ ./test-l1vm
</pre>

<b>
NEW: I did develop a linter to check if the function call uses the right function arguments variables.
You can write this in comments for the linter to check this. See the post on my blog!
</b>

Here is the output of my fractalix fractal program using the SDL module:<br>

![L1VM-fractalix](https://midnight-koder.net/blog/assets/l1vm/L1VM-fractal.png)

<br>

Here is the pov-edit program GUI for my LED POV kit: <br>

![L1VM-fractalix](https://midnight-koder.net/blog/assets/l1vm/L1VM-pov-edit-01.png)

<br>

[L1VM - pov edit flash video](https://midnight-koder.net/blog/assets/l1vm/LED-POV-8-red-flash.mp4)

<br>

A more advanced demo program is "prog/people-object.l1com". It shows how to use a memory object and pointers.
I did change the program to use a variable end "main" part by using:

<pre>
#var ~ main
</pre>

This is more safe because now every variable in main ends with "main". Take a look at this example.

<b>NEW features</b> <br>
Now legal value ranges for variables can be set:

```
(x x_min x_max range)
```

If x is outside of the range then it results in a runtime exception. <br>

Now in the infix/RPN parser you can write a short name for the target variable: _ like this:

```
{target = (_ + x + (_ * y))}
```

Every _ will be replaced by "target"!
See prog/hello-target-shortname.l1com. <br>


How to install "pov-edit" and the internet jargon file database "jargon":
Go into the distribution folder and download and install my gpg key:

```
$ ./get-koder77-key.sh
```

Then install the programs with: <br>

```
$ ./download.sh pov-edit
```

```
$ ./download.sh jargon
```
<br>

The L1VM has a full SDL graphics/GUI module for writing desktop applications!
And you can write your own modules.
Here is the full standard modules list: <br>

<h2>Modules</h2>
<pre>
Cells - linked neural networks with FANN library
ciphersaber - encrypt/decrypt functions
crypto - libsodium encrypt/decrypt functions
endianess - convert to big endian, or little endian functions
fann - FANN neural networks
file - file module - read/write files
filetools - file functions like copy, create directory, etc.
genann - neural networks module
gpio - Raspberry Pi GPIO module
math - some math functions
math-nofp - math module for use without FPU
math-vect - math on arrays functions
mem - memory allocating for arrays and vectors
mem-obj - memory functions to store different variables into one memory array
mem-vect - C++ vector memory library
mpfr-c++ - MPFR floating point big num library
nanoid - nano ID, create unique IDs
net - TCP/IP sockets module
pigpio - Raspberry Pi GPIO module
process - start a new shell process
rs232-libserialport - RS232 serial port using libserialport
rs232 - RS232 serial port module
sdl 2.0 - graphics primitves module, like pixels, lines..., and GUI with buttons, lists, etc.
string - some string functions
string-unicode - unicode support, convert unicode to string
time - get time and date
</pre>

<h2>Credits</h2>
Thanks goes to: <br>
Florian Bruder (sportfloh) for helping on the macOS port. <br>
Cedric Kienzler (cedi) for helping setup GitHub Actions .yaml script. <br>
Andreas Weber (Andy1978) for fixing some serious bugs. <br>
The team behind SDL and A. Schiffler (ferzkopp) for their great work! <br>
The author of genann networks library: Lewis Van Winkle (codeplea). <br>
The authors of the libfann library. <br>
The author of MPFR C++: Pavel Holoborodko. <br>
The author of the RS232 library I use: Teunis van Beelen. <br>
The authors of libserialport: the sigrok team. <br>
The authors of mt19937-64.c random number generator :Takuji Nishimura and Makoto Matsumoto. <br>
The author of Wiring Pi, the Raspberry Pi GPIO library: drogon. <br>
The author of libasmjit, JIT-compiler: Petr Kobalicek. <br>
The authors of www.tutorialspoint.com, for their polish math notation parser example! <br>
The author of the fp16 fixed floating point library: https://github.com/kmilo17pet/fp16 <br>
The team of the libsodium library. I use for generating random numbers. <br>
The author of the nanoid ID generating library. <br>
The JuliaStrings team for the utf8proc library for unicode support. <br>
<br><br>

Without them this L1VM project would not be possible! Thank you! <br>
----------------------- <br>

<h2>New: variable range checks</h2>
Now legal value ranges for variables can be set:

```
(x x_min x_max range)
```

If x is out of range then an exception is executed to exit the program.
This was inspired by Ada!
See prog/hello-ranges.l1com!

<h2>l1pre - the preprocessor</h2>
The new "l1pre" preprocessor can be used to define macros and include files.
See the new "include-lib" directory with all header files. The "prog/hello-6.l1com" example shows how to use this!

<h2>reversed polish math notation</h2>
Math expressions in: { }, are parsed as reversed polish notation:

{a = x y + z x * *}

is the same as: "a = x + y * z * x"
This needs no brackets for complex math expressions!
See "prog/hello-4.l1com" example!

<h2>"(loadreg)" setting</h2>
it is set automatically after a function call, or the "stpop" opcodes.
Or you can do it in the code.

<h2>SDL 2.0!</h2>
Finally I ported the SDL gfx/GUI library to SDL 2.0!
The source code is in: vm/modules/sdl-2.0/

I did remove the SDL 1.2 code from this repo. So for now the SDL 2.0 libraries are needed
to build my L1VM SDL library.

You need to build vm-sdl2/ VM sources to use the new SDL 2.0 gfx/GUI library.
This source of the L1VM is linked with the SDL2 libraries.


<h2>1. Configuration</h2>
<b>NEW</b>
Before installation you have to add the following lines to your "~/.bashrc" bash config file: <br>

```
export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"
```

You also can run the ```set-path.sh``` script to do this.

Configure the file access for SANDBOX mode or not, and set your /home directory name:
include/settings.h:

<pre>
// SANDBOX FILE ACCESS
#define SANDBOX                 1			// secure file acces: 1 = use secure access, 0 = OFF!!
#define SANDBOX_ROOT			"/l1vm/"	// in /home directory!, get by $HOME env variable
</pre>

And you can choose if the array variable bounds check and math checks should be run:

<pre>
// data bounds check exactly
#define BOUNDSCHECK             1
</pre>

Math limits checks, on/off:

<pre>
// division by zero checking
#define DIVISIONCHECK           1

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
Note: you must be in the sudoers list to run the installation scripts.
To do this just run this as root (on Linux for example):

<pre>
# /usr/sbin/usermod -aG sudo username
</pre>

Logout of your current session. And login again.
Now you can run the installation scripts with the sudo command.

<h3>Debian Linux</h3>
Use: install-debian.sh or install-jit-debian.sh

<h3>Fedora Linux</h3>
Use: install-zerobuild-fedora.sh or install-jit-zerobuild-fedora.sh

<h3>Windows 10, 11 WSL</h3>
You need to edit: "include/settings.h":

```
#define WINDOWS_10_WSL 1

#define DIVISIONCHECK  0
```

You need to add the following lines to your ".bashrc":

```
export LD_LIBRARY_PATH="$HOME/bin:/lib:/usr/lib:/usr/local/lib"
export XDG_RUNTIME_DIR="$HOME/xdg"
export RUNLEVEL=3
export DISPLAY=$(grep -m 1 nameserver /etc/resolv.conf | awk '{print $2}'):0
```

Create "xdg" direcory. In Home folder open shell:

```
$ mkdir xdg
```


Use: install-wsl-debian.sh or install-wsl-debian-jit.sh

Make the bash install script executable by:

<pre>
$ chmod +x ./install-wsl-debian.sh
</pre>

And run it:
<pre>
$ ./install-wsl-debian.sh
</pre>

For graphics/GUI programs you need a Xserver installed.
I use VcXsrv as the X11 server. Start VcXsrv first then the WSL shell!

Note: the install scripts automatically install clang C compiler and my zerobuild build tool!

<h3>Windows MSYS2</h3>
You need to edit: "include/settings.h":

```
#define DIVISIONCHECK  0

#define CPU_SET_AFFINITY   0

```

And you need to put this at your "/etc/bash.bashrc":

```
export PATH="$PATH:/mingw64/bin:$HOME/bin"
export LD_LIBRARY_PATH="$HOME/bin:$LD_LIBRARY_PATH"
```

Create the "~/bin" directory:

```
$ mkdir bin
```

Now you can run the installation script:

```
$ ./install-zerobuild-msys2.sh
```


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

<h3>macOS</h3>
The GitHub CI build now uses macOS 11 to build the VM.
UPDATE now the modules are working!! I had to build them as "bundle"!
NEW: macOS build scripts made together with sportfloh.
And cedi did write the .yaml GitHub Actions file.
See main directory: "install-zerobuild-macos.sh".

What works right now: l1asm, l1com, l1pre and the l1vm. <br>
And all the following standard modules: <br>
endianess, fann, file, genann, math, mem, mmpfr (high precision math library), net, process, string, time, sdl-2.0 <br>
librs232 (serialport by libserialport) <br><br>

sdl-2.0: prog/lines.l1com now works! I had to call a SDL event function in the delay loop! <br>
(:sdl_get_mouse_state !) <br>
This is needed to avoid errors on macOS. <br>

Before installation you have to add the following lines to your "~/.bashrc" bash config file: <br>

```
export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"
```

<h3>Haiku OS</h3>
I did write an installer script for the current beta version of Haiku.
Programs which use multithreading can't run! This results in a "segmentation fault" in the OS.
Maybe this gets fixed by newer Haiku beta releases.

You need to edit the ```include/settings.h``` file:

```
#define DIVISIONCHECK 0

#define CPU_SET_AFFINITY 0
```

<b>Update:</b> I fixed the installer scripts. Now the modules can be load by the VM too!

Now you are ready to run the install script!


<h3>OpenBSD / FreeBSD</h3>
You have to edit the settings.h include file:

<pre>
// division by zero checking
#define DIVISIONCHECK           0

// switch on on Linux
#define CPU_SET_AFFINITY		0
</pre>

And change the linker flags on the VM zerobuild file: "vm/zerobuild-nojit.txt"

<pre>
lflags = "-lm -lpthread -Wl,--export-dynamic"
</pre>

Add the "-L/usr/local/lib" path to the "zerobuild.txt" make files linker line:
on: vm/modules/fann, vm/modules/math, vm/modules/crypto, vm/modules/rs232-libserialport and vm/modules/sdl-2.0

<pre>
lflags = "-shared -lm -L/usr/local/lib -lfann"
</pre>

Now replace every "SDL_BYTEORDER" in vm/modules/sdl-2.0/sdl.c by "_BYTE_ORDER".

Currently the MPFR module can't be build: "features.h" include is missing!
However: there is a workaround:

Copy the "features.h" header in vm/modules/mpfr-c++/openbsd/ to "/usr/include".
This header was patched by me to make the MPFR module build.


Now you can start the build with:

OpenBSD: <br>
<pre>
sh ./install-zerobuild-openbsd.sh
</pre>

FreeBSD: <br>
Install the "sudo" program as root: ```pkg install sudo```.
Then as a normal user:

<pre>
sh ./install-zerobuild-freebsd.sh
</pre>

Edit your "~/.bashrc" config file, add the following lines:

```
export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"
```

<h3>NetBSD</h3>
You have to edit the settings.h include file:

<pre>
// division by zero checking
#define DIVISIONCHECK           0

// switch on on Linux
#define CPU_SET_AFFINITY		0
</pre>

Then you can run the install script for NetBSD.
Note: the SDL module can't be build. I have to find out what the error is.
And the script for building the programs for L1VM fails. It shows a out of memory error message.
If you use NetBSD and could take a look after it, this would be nice!

Edit your "~/.bashrc" config file, add the following lines:

```
export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"
```

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
    export PATH=$HOME/l1vm/bin:$PATH
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

<h3>compile using preprocessor</h3>
Compile "lib/sdl-lib.l1com" example:
<pre>
$ ./l1vm-build.sh lib/sdl-lib
</pre>
Always the full name must be used by the preprocessor l1pre!
