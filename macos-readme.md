L1VM macOS README 2021-12-18
============================
UPDATE: I made the following modules build on macOS:
endianess, fann, file, genann, math, mem, mmpfr (high precision math library), net, process, rs232, string, time
librs232 (serialport by libserialport) <br><br>

The sdl-2.0 module can not be used. The SDL library is called from a non main thread!
Resulting in the following error message:
sdl_open_screen: ERROR window can't be opened: NSWindow drag regions should only be invalidated on the Main Thread! <br><br>

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
