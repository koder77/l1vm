At the moment only the Fedora podman build works!

edora Linux install in podman
==============================
$ podman pull fedora
$ podman run -ti --name fedora --hostname fedora-host --memory 512mb --network host fedora /bin/sh

In Fedora podman container:
--------------------------
$ cd /home
$ mkdir user
$ cd user
$ git clone https://github.com/koder77/l1vm.git
$ cd l1vm
$ sh l1vm-podman/install-jit-zerobuild-fedora-podman.sh
$ ldconfig

Now you should be ready!!


Alpine Linux install in podman experimental
===========================================
NOTE: the compiler runs, but the VM does "segfault"!!
So the VM doesn't run on Alpine!
If you know how to FIX it, then write me a mail please.

$ podman pull alpine:latest
$ podman run -ti --name alpine --hostname alpine-host --memory 256mb --network host alpine:latest /bin/sh

In Alpine podman container:
---------------------------
$ apk add git
$ apk add nano
$ git clone https://github.com/koder77/l1vm.git
$ cd l1vm
$ nano vm/jit.h
----------------------------------
In Editor set:

#define JIT_COMPILER 0

And save it: "ctrl + O"
And exit the nano editor: "ctrl + X"
------------------------------------
Now you should be ready to run the L1VM Alpine podman script:

$ sh l1vm-podman/install-jit-zerobuild-alpine-podman.sh

Thats it!
