#!/bin/bash
set -e
cd ../vm/modules/endianess
chmod +x *.sh
./make-endianess-mod.sh
cd ../fann
chmod +x *.sh
./make-fann-mod.sh
cd ../file
chmod +x *.sh
./make-file-mod.sh
cd ../genann
chmod +x *.sh
./make-genann-mod.sh
cd ../math
chmod +x *.sh
./make-math-mod.sh
cd ../mpfr-c++
chmod +x *.sh
./make-mpfr.sh
cd ../net
chmod +x *.sh
./make-net-mod.sh
cd ../process
chmod +x *.sh
./make-process-mod.sh
cd ../rs232
chmod +x *.sh
./make-rs232-mod.sh
cd ../sdl-2.0
chmod +x *.sh
./make-sdl-mod.sh
cd ../string
chmod +x *.sh
./make-string-mod.sh
cd ../time
chmod +x *.sh
./make-time-mod.sh
cd ../mem
chmod +x *.sh
./make-mem-mod.sh

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
# sudo cp libl1vm* /usr/local/lib
