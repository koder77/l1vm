L1VM OSv capstan build
======================
In this directory is a build environment for OSv unikernel OS:
[OSv](https://github.com/cloudius-systems/osv)

This example shows how to use a module with a OSv L1VM build.

You have to switch SDL support off in the include/global.h file.
And also must install "capstan" build tool. See OSv documentation!

The build script builds the L1VM as a shared library and builds a OSv Qemu image
in ~/.capstan. This is a hidden directory in your /home directory.
You have to switch on "show hidden directories" in your file manager!

At the end the image is run with Qemu.

In this demo build the "string-lib" program is executed.
And in this example the VM opens the string module which is also included in the OSv image.

The image is run via Qemu at the end of the script!
Now you should have a qemu .img file in your /home directory: ~/.capstan!
