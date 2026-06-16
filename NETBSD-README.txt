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

And if you get missing library errors, that a library can not be load. Then use "ldd libraryname.so" to find out what is missing.
You have to set the library path in the ".shrc" file:

export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:/usr/pkg/lib:/usr/lib:/usr/X11R7/lib:$LD_LIBRARY_PATH"
export L1VM_ROOT="$HOME/l1vm/"

Now reboot and the new values should be set.

Have fun!
