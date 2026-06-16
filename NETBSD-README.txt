To use the L1VM binaries on NetBSD do the following memory size changes as root:

# ulimit -s 16384
# ulimit -d 1000000

Then open a new shell and now you can run the L1VM binaries without memory errors!

You can set the limits in the "/etc/login.conf" file:
as root:

# nano /etc/login.conf


default:\
:datasize-max=4096M:\
:datasize-cur=4096M:\

:stacksize-cur=16M:\


To set the values, do this:

# cap_mkdb /etc/login.conf

In the "include/settings.h" file set this line:

#define NETBSD_RAM 1

And if you get missing library errors, that a library can not be load. Then use "ldd libraryname.so" to find out what is missing:

I had to copy the libsodium:

$ sudo cp /usr/pkg/lib/libsodium.so /usr/lib/libsodium.so.26

To make the math library "libl1vmmath.so" loading.

Now reboot and the new values should be set.

Have fun!
