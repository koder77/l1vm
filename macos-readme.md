L1VM macOS README
=================
This is experimental stuff about make a macOS build of my L1VM.
This is not ready yet!!

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

This is not really tested yet. Feel free to contact me about this if you are a macOS user.
So we can finish this!
S
