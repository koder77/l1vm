#!/bin/sh
echo "capstan must be installed to run this script..."
echo "L1VM must be configured without SDL support to build OSv l1vm!"
echo "copy buildfile"
cp zerobuild-osv.txt ../../vm
cd ../../vm
echo "building l1vm-osv.so"
zerobuild zerobuild-osv.txt force
cp l1vm-osv.so ../container/l1vm-osv-capstan-webserver
cd ../prog
cp webserver-thread-local.l1obj ../container/l1vm-osv-capstan-webserver
cd ../container/l1vm-osv-capstan-webserver
cp ../../vm/modules/net/libl1vmnet.so .
cp ../../vm/modules/string/libl1vmstring.so .
cp ../../website/index.html .
cp ../../website/404.html .
capstan package pull osv.bootstrap
capstan package compose l1vm
echo "running OSv image using qemu..."
capstan run l1vm
