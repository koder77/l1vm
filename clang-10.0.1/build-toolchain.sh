#!/bin/bash
echo "L1VM - build clang 10.0.1 toolchain 1.3"
#!get files from llvm.org
echo "downloading archives from llvm.org github..."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/llvm-10.0.1.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/clang-10.0.1.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/llvm-10.0.1.src.tar.xz.sig
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/clang-10.0.1.src.tar.xz.sig
echo "Downloading llvm.org gpg key."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-9.0.1/tstellar-gpg-key.asc
#!check signed archives
gpg --import tstellar-gpg-key.asc
if [ $? -gt 0 ]
then
	echo "ERROR: importing pgp key! Stopping!"
	exit 1
fi
echo "The key was successfully added to your keyring!"
gpg --verify clang-10.0.1.src.tar.xz.sig clang-10.0.1.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying clang archive! Stopping!"
	exit 1
fi

gpg --verify llvm-10.0.1.src.tar.xz.sig llvm-10.0.1.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying llvm archive! Stopping!"
	exit 1
fi

echo "All init stuff done! Let's untar the archives..."
tar -xJf llvm-10.0.1.src.tar.xz

tar -xJf clang-10.0.1.src.tar.xz -C llvm-10.0.1.src/tools/
mv llvm-10.0.1.src/tools/clang-10.0.1.src llvm-10.0.1.src/tools/clang

echo "creating build directory..."
cd llvm-10.0.1.src
mkdir build
cd build
echo "configure..."
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/l1vm-clang-10.0.1 ../
echo "make..."
make -j4
echo "install..."
make install
echo "finished! now the L1VM VM clang toolchain is installed!"
cd ../..
echo "copying makefiles..."
cp make/make-assemb.sh ../assemb/make-assemb.sh
cp make/zerobuild-assemb.txt ../assemb/zerobuild-assemb.txt
cp make/make-comp.sh ../comp/make-comp.sh
cp make/zerobuild-comp.txt ../comp/zerobuild-comp.txt
cp make/make-vm.sh ../vm/make-vm.sh
cp make/zerobuild-nojit-vm.txt ../vm/zerobuild-nojit-vm.txt
cp make/zerobuild-vm.txt ../vm/zerobuild-vm.txt
echo "finished copying makefiles. All OK!"
echo "L1VM - build clang 10.0.1 toolchain 1.3"
exit 0
