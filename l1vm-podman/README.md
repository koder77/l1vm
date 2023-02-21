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
$ dnf install nano <br>
$ adduser user <br>
$ passwd <br>
$ passwd user <br>
$ usermod -aG wheel user <br>
$ sudo su user <br>
$ cd /home/user <br>
$ git clone https://github.com/koder77/l1vm.git <br>
$ cd l1vm/include <br>
$ nano global.h <br><br>

Type "ctrl + O" to save the file, and "ctrl + X" to exit the editor. <br>

cd ..

Now edit vm/jit.h: <br>
cd vm/jit.h <br>
nano jit.h <br>

#define JIT_COMPILER          1 <br><br>

Type "ctrl + O" to save the file, and "ctrl + X" to exit the editor. <br>

cd .. <br>
mv l1vm l1vm-work <br>
<br>
$ mkdir /home/user/l1vm <br>
 cd l1vm-work <br>
$ sh l1vm-podman/install-jit-zerobuild-fedora-podman.sh <br>
$ sudo ldconfig <br>

Now you should be ready to run L1VM! <br>
After "exit" the conatiner you can commit it and create an new image with my L1VM in it! <br><br>

Run the following command with your container-ID: <br>
podman commit [container-ID] l1vm-fedora-image <br><br>

Now podman should have created a new image "l1vm-fedora-image". <br>
You can run this image any time if needed! <br><br>

Have some fun!!!
