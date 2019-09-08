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

In this demo build the "math-lib" program is executed.
And in this example the VM opens the math module which is also included in the OSv image.

To run the build script you have to build some things first:
Go into the l1vm directory and open a shell.
You have to type in the commands after the "$" sign:

<pre>
l1vm $ comp/l1com lib/math-lib
l1vm $ assemb/l1asm lib/math-lib
l1vm $ cd vm/modules/math
l1vm/modules/math $ ./make-math-mod.sh
l1vm/modules/math $ cd ../../../
l1vm $ cd l1vm-osv-capstan-lib
l1vm/l1vm-osv-capstan-lib $ ./build.sh
</pre>

The image is run via Qemu at the end of the script!
Now you should have a qemu .img file in your /home directory: ~/.capstan!
