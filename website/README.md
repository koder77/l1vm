L1VM webserver demo README
==========================
Just copy the files here into: /home/username/l1vm.
The prog/webserver.l1com program will search it's files there if SANDBOX mode
in include/global.h is set!

To run the webserver as Root:

# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/ussrname/bin && l1vm prog/webserver

Note you should use some kind of chroot to run it as a server for port 80.
Or you change the port number to something higher.
