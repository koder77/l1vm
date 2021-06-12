#!/bin/bash
echo "L1VM - build clang 12.0.0 toolchain"
#!get files from llvm.org
echo "downloading archives from llvm.org github..."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/llvm-12.0.0.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/clang-12.0.0.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/llvm-12.0.0.src.tar.xz.sig
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/clang-12.0.0.src.tar.xz.sig

echo "All init stuff done! Let's untar the archives..."
tar -xJf llvm-12.0.0.src.tar.xz

tar -xJf clang-12.0.0.src.tar.xz -C llvm-12.0.0.src/tools/
mv llvm-12.0.0.src/tools/clang-12.0.0.src llvm-12.0.0.src/tools/clang

echo "creating build directory..."
cd llvm-12.0.0.src
mkdir build
cd build
echo "configure..."
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/l1vm-clang-12.0.0 ../
echo "make..."
make -j4
echo "install..."
make install
echo "finished! now the L1VM VM clang toolchain is installed!"
exit 0
