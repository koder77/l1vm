L1VM OSV capstan build
======================
In this directory is a build environment for OSv unikernel OS:
[OSv](https://github.com/cloudius-systems/osv)

You have to switch SDL support off in the include/global.h file.
And also must install "capstan" build tool. See OSv documentation!

The build script builds the L1VM as a shared library and builds a OSv Qemu image
in ~/.capstan. This is a hidden directory in your /home directory.
You have to switch on "show hidden directories" in your file manager!

At the end the image is run with Qemu.

In this demo build the "hello-2" program is executed.
This is a starting point for doing more complex stuff.
