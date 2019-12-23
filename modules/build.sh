#!/bin/bash
cd ../vm/modules/endianess
./make-endianess-mod.sh
cd ../fann
./make-fann-mod.sh
cd ../file
./make-file-mod.sh
cd ../genann
./make-genann-mod.sh
cd ../math
./make-math-mod.sh
cd ../mpfr-c++
./make-mpfr.sh
cd ../net
./make-net-mod.sh
cd ../process
./make-process-mod.sh
cd ../rs232
./make-rs232-mod.sh
cd ../sdl
./make-sdl-mod.sh
cd ../string
./make-string-mod.sh
cd ../time
./make-time-mod.sh

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
cp ../vm/modules/sdl/libl1vm* .
cp ../vm/modules/string/libl1vm* .
cp ../vm/modules/time/libl1vm* .

# sudo cp libl1vm* /usr/local/lib
