L1VM macOS README 2021-12-18
============================
UPDATE: I made the following modules build on macOS:
endianess, fann, file, genann, math, mem, mmpfr (high precision math library), net, process, rs232, string, time
librs232 (serialport by libserialport) <br><br>

The sdl-2.0 module can not be used. <br>
Resulting in the following error message: <br>
running lines SDL demo... <br>
2021-12-18 19:54:21.241 l1vm[18905:51693] Attempting to add observer to main runloop, but the main thread has exited. This message will only log once. Break on _CFRunLoopError_MainThreadHasExited to debug. <br><br>

Now the SDL library is used from the main thread, but still not working right! <br>
If you know how to fix this then you could help me! <br><br>

I did find out that on macOS .so libraries must be build as "bundle". <br><br>

If you want to try it, then here is what I found out so far:
You have to edit "include/settings.h" to:

```
// division by zero checking
#define DIVISIONCHECK           0

// switch on on Linux
#define CPU_SET_AFFINITY        0
```

The next step is to create a "bin/" and a "l1vm/" directory in your home directory.
The L1VM programs and libraries are installed into the "bin" directory and the data into "l1vm".

You now have to add the "~/bin" directory to your PATH env variable.
Go to "/etc/paths.d" directory and create a new file: "l1vm".
In this file insert the following line:

```
export PATH="$HOME/bin:$PATH"
```

Now you can try to build on macOS:

```
$ ./install-zerobuild-macos.sh
```

If you get error messages while building the "mpfr-c++" library then try to copy the file:
"vm/modules/mpfr-c++/mpreal.h" to "/usr/local/include". My "mpreal.h" is a patched version with some fixes in it. See "vm/modules/mpfr-c++/README.md"
