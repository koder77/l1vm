L1VM macOS README
=================
This is experimental stuff about make a macOS build of my L1VM.
This is not ready yet!!

You need to install the following packages with Homebrew:

```
SDL2-devel, SDL2_gfx, SDL2_image, SDL2_ttf, SDL2_mixer
fann-devel
mpfr-devel
cmake
make
git
openssl-devel
```

If you want to try it, then here is what I found out so far:
You have to edit "include/settings.h" to:

```
// division by zero checking
#define DIVISIONCHECK           0

// switch on on Linux
#define CPU_SET_AFFINITY		0
```

Now you can try to build on macOS:

```
$ ./install-zerobuild-macos.sh
```

If you get error messages while building the "mpfr-c++" library then try to copy the file:
"vm/modules/mpfr-c++/mpreal.h" to /usr/local/include. My "mpreal.h" is a patched version with some fixes in it. See "vm/modules/mpfr-c++/README.md"

This is not really tested yet. Feel free to contact me about this if you are a macOS user.
So we can finish this!
