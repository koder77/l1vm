#!/bin/bash
set -e
cd ../vm/modules/endianess
zerobuild force
cd ../fann
zerobuild force
cd ../file
zerobuild force
cd ../genann
zerobuild force
cd ../math
zerobuild force
cd ../mpfr-c++
zerobuild force
cd ../net
zerobuild force
cd ../process
zerobuild force
cd ../rs232
zerobuild force
cd ../sdl-2.0
zerobuild force
cd ../string
zerobuild force
cd ../time
zerobuild force
cd ../mem
zerobuild force
cd ../l1vm-data
zerobuild force

cd ../../../modules
cp ../vm/modules/endianess/libl1vm* .
cp ../vm/modules/fann/libl1vm* .
cp ../vm/modules/file/libl1vm* .
cp ../vm/modules/genann/libl1vm* .
cp ../vm/modules/math/libl1vm* .
cp ../vm/modules/mpfr-c++/libl1vm* .
cp ../vm/modules/net/libl1vm* .
cp ../vm/modules/process/libl1vm* .
cp ../vm/modules/rs232/libl1vm* .
cp ../vm/modules/sdl-2.0/libl1vm* .
cp ../vm/modules/string/libl1vm* .
cp ../vm/modules/time/libl1vm* .
cp ../vm/modules/mem/libl1vm* .
cp ../vm/modules/l1vm-data/libl1vm* .

# sudo cp libl1vm* /usr/local/lib
