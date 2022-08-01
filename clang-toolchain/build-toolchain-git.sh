#!/bin/bash
echo "L1VM - build clang git toolchain"
#!get files from llvm.org
echo "downloading archives from llvm.org github..."
#git clone https://github.com/llvm/llvm-project.git
cd llvm-project
cmake -S llvm -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/l1vm-clang-git ..
cmake --build build -j 4
cd build
make install
exit 0
