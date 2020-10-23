Alpine Linux install in podman
==============================
$ podman pull alpine:latest
$ podman
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

$ sh ./install-jit-zerobuild-alpine-podman.sh

Thats it!
