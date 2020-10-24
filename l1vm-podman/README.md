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
$ ldconfig <br>

Now you should be ready to run L1VM!
