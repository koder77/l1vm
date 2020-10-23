At the moment only the Fedora podman build works!

Fedora Linux install in podman
==============================
$ podman pull fedora <br>
$ podman run -ti --name fedora --hostname fedora-host --memory 512mb --network host fedora /bin/sh <br><br>

In Fedora podman container:
--------------------------
$ dnf install git <br>
$ dnf install passwd <br>
$ dnf install cracklib <br>
$ dnf install cracklib-dicts <br>
$ adduser user <br>
$ passwd <br>
$ passwd user <br>
$ usermod -aG wheel user <br>
$ sudo su user <br>
$ cd /home/user <br>
$ git clone https://github.com/koder77/l1vm.git <br>
$ cd l1vm <br>
$ sh l1vm-podman/install-jit-zerobuild-fedora-podman.sh <br>
$ ldconfig <br><br>

Now you should be ready!!
<br>

Alpine Linux install in podman experimental
===========================================
NOTE: the compiler runs, but the VM does "segfault"!!
So the VM doesn't run on Alpine!
If you know how to FIX it, then write me a mail please.

$ podman pull alpine:latest <br>
$ podman run -ti --name alpine --hostname alpine-host --memory 256mb --network host alpine:latest /bin/sh <br><br>

In Alpine podman container:
---------------------------
$ apk add git <br>
$ apk add nano <br>
$ git clone https://github.com/koder77/l1vm.git <br>
$ cd l1vm <br>
$ nano vm/jit.h <br><br>

In Editor set:

#define JIT_COMPILER 0

And save it: "ctrl + O"
And exit the nano editor: "ctrl + X"

Now you should be ready to run the L1VM Alpine podman script:

$ sh l1vm-podman/install-jit-zerobuild-alpine-podman.sh

Thats it!
