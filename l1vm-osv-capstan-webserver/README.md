L1VM OSv capstan webserver build
================================

In this directory is a build environment for OSv unikernel OS:
[OSv](https://github.com/cloudius-systems/osv)

You have to switch SDL support off in the "include/global.h" file.
And also must install "capstan" build tool. See OSv documentation!

The build script builds the L1VM as a shared library and builds a OSv Qemu image
in ~/.capstan. This is a hidden directory in your /home directory.
You have to switch on "show hidden directories" in your file manager!

At the end the image is run with Qemu.

In this demo build the "webserver" program is executed ("prog/webserver.l1com").
This is a starting point for doing more complex stuff.

BUILD NOTE:
===========
First make sure that SANDBOX mode directory is : "" in "include/global.h" and "SANDBOX" is set to "TRUE"!
Then you have to set the IP address for the webserver to the one which is used by the OSv unikernel
at runtime. Note that the OSv kernel uses ethernet port for network!

If everything was configured right you should see the /index.html page!
