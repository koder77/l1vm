#!/bin/sh
#!sleep 30
gpio export 17 in
gpio export 27 in
export LD_PRELOAD="/usr/local/lib/libv4l/v4l2convert.so"
#!export LD_PRELOAD="/usr/local/lib/libv4l/v4l1compat.so"
cd /home/pi/l1vm-1.1-2/l1vm
#! sudo chmod 666 /dev/ttyS0
vm/l1vm lib/robotcontrol-4.3
#!sleep 20
#!sudo su -c 'shutdown -h now'
