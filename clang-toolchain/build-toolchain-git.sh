#!/bin/bash
echo "L1VM - build Clang git toolchain"
echo "Cloning latest Clang from GitHub..."
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
cmake -S llvm -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lld" -DCMAKE_INSTALL_PREFIX=~/l1vm-clang-git ..
cmake --build build -j 16
cd build
make install
exit 0
