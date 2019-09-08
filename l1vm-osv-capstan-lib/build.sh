#!/bin/sh
echo "capstan must be installed to run this script..."
echo "L1VM must be configured without SDL support to build OSv l1vm!"
echo "copy buildfile"
cp zerobuild-osv.txt ../vm
cd ../vm
echo "building l1vm-osv.so"
zerobuild zerobuild-osv.txt force
cp l1vm-osv.so ../l1vm-osv-capstan-test
cd ../lib
cp math-lib.l1obj ../l1vm-osv-capstan-test
cd ../l1vm-osv-capstan-test
cp ../vm/modules/math/libl1vmmath.so .
capstan package pull osv.bootstrap
capstan package compose l1vm
echo "running OSv image using qemu..."
capstan run l1vm
