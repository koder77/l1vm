MAKE TOOLCHAIN
==============
In this directory is a usefull script that downloads and installs a clang 12.0 toolchain in
your home directory: "l1vm-clang-12.0.0/".

To run a full build you have to run: "install-toolchain.sh" in the root directory of L1VM.

The needed env variables are exported to "CC" (clang) and "CCPP" (clang++).

NEW: "build-toolchain-git.sh" clones the latest version of Clang from GitHub and builds it. It will be installed into your home directory: "l1vm-clang-git".
To use it you have to set the compiler path in the build script:

```
export CC=~/l1vm-clang-git/bin/clang
export CCPP=~/l1vm-clang-git/bin/clang++
```
