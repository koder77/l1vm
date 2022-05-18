L1VM macOS README 2022-05-18
============================
UPDATE: I made the following modules build on macOS:
endianess, fann, file, genann, math, mem, mmpfr (high precision math library), net, process, rs232, string, time, sdl-2.0
librs232 (serialport by libserialport) <br><br>

sdl-2.0: prog/lines.l1com now works! I had to call a SDL event function in the delay loop! <br>
(:sdl_get_mouse_state !) <br>
This is needed to avoid errors on macOS. <br>

I get this errror messsage: <br>
running lines SDL demo... <bR>
Assertion failed: (!needsMainQueue() || pthread_main_np() != 0), function invoke, file /System/Volumes/Data/SWE/macOS/BuildRoots/220e8a1b79/Library/Caches/com.apple.xbs/Sources/SkyLight/SkyLight-588.8/SkyLight/Services/PortStreams/CGSDatagramReadStream.cc, line 147. <br><br>

So now the SDL module seems to run  to the end of the lines.l1com program. <br>
If you are a macOS user then you could run the build script and tell me if the lines demo
shows the window and the lines! <br>

If you want to try it, then here is what I found out so far:
You have to edit "include/settings.h" to:

```
// division by zero checking
#define DIVISIONCHECK           0

// switch on on Linux
#define CPU_SET_AFFINITY        0
```
In the following lines "$HOME" means your home directory path.
The next step is to create a "$HOME/bin" and a "$HOME/l1vm" directory in your home directory.
The L1VM programs and libraries are installed into the "bin" directory and the data into "l1vm".

You now have to add the "$HOME/bin" directory to your PATH env variable.
Add the following lines to your "$HOME/.bashrc" config file:

```
export PATH="$HOME/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/bin:$LD_LIBRARY_PATH"
```

And write the following lines into your "~/.bash_profile"

```
if [ -r ~/.bashrc ]; then
   source ~/.bashrc
fi
```

Now you can try to build on macOS:

```
$ ./install-zerobuild-macos.sh
```

If you get error messages while building the "mpfr-c++" library then try to copy the file:
"vm/modules/mpfr-c++/mpreal.h" to "/usr/local/include". My "mpreal.h" is a patched version with some fixes in it. See "vm/modules/mpfr-c++/README.md"
