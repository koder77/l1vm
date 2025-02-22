#!/bin/bash
set -e
cd ../vm/modules/endianess
zerobuild zerobuild-win.txt force
cd ../fann
zerobuild zerobuild-win.txt force
cd ../file
zerobuild zerobuild-win.txt force
cd ../genann
zerobuild zerobuild-win.txt force
cd ../math
zerobuild zerobuild-win.txt force
cd ../mpfr-c++
zerobuild zerobuild-win.txt force
cd ../net
zerobuild zerobuild-win.txt force
cd ../process
zerobuild zerobuild-win.txt force
cd ../rs232-libserialport
zerobuild zerobuild-win.txt force
cd ../sdl-2.0
zerobuild zerobuild-win.txt force
cd ../string
zerobuild zerobuild-win.txt force
cd ../time
zerobuild zerobuild-win.txt force
cd ../mem
zerobuild zerobuild-win.txt force
cd ../../../modules
cp ../vm/modules/endianess/libl1vm* .
cp ../vm/modules/fann/libl1vm* .
cp ../vm/modules/file/libl1vm* .
cp ../vm/modules/genann/libl1vm* .
cp ../vm/modules/math/libl1vm* .
cp ../vm/modules/mpfr-c++/libl1vm* .
cp ../vm/modules/net/libl1vm* .
cp ../vm/modules/process/libl1vm* .
cp ../vm/modules/rs232-libserialport/libl1vm* .
cp ../vm/modules/sdl-2.0/libl1vm* .
cp ../vm/modules/string/libl1vm* .
cp ../vm/modules/time/libl1vm* .
cp ../vm/modules/mem/libl1vm* .
# sudo cp libl1vm* /usr/local/lib
