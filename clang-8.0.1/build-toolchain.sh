#!/bin/bash
echo "L1VM - build clang toolchain 1.0"
#!get files from llvm.org
echo "downloading archives from llvm.org github..."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1/llvm-8.0.1.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1/cfe-8.0.1.src.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1/llvm-8.0.1.src.tar.xz.sig
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-8.0.1/cfe-8.0.1.src.tar.xz.sig

#!get llvm public key
echo "Downloading llvm.org gpg key."
wget http://releases.llvm.org/7.0.1/tstellar-gpg-key.asc
#!check signed archives
gpg --import tstellar-gpg-key.asc
if [ $? -gt 0 ]
then
	echo "ERROR: importing pgp key! Stopping!"
	exit 1
fi
echo "The key was successfully added to your keyring!"
gpg --verify cfe-8.0.1.src.tar.xz.sig cfe-8.0.1.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying cfe archive! Stopping!"
	exit 1
fi
gpg --verify llvm-8.0.1.src.tar.xz.sig llvm-8.0.1.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying llvm archive! Stopping!"
	exit 1
fi

echo "All init stuff done! Let's untar the archives..."
tar -xJf llvm-8.0.1.src.tar.xz

tar -xJf cfe-8.0.1.src.tar.xz -C llvm-8.0.1.src/tools/
mv llvm-8.0.1.src/tools/cfe-8.0.1.src llvm-8.0.1.src/tools/clang

cd llvm-8.0.1.src
mkdir build
cd build
echo "configure..."
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/l1vm-clang ../
echo "make..."
make -j4
echo "install..."
make install
echo "finished! now the L1VM VM clang toolchain is installed!"
cd ../..
echo "copying makefiles..."
cp make/make-assemb.sh ../assemb/make.sh
cp make/zerobuild-assemb.txt ../assemb/zerobuild.txt
cp make/make-comp.sh ../comp/make.sh
cp make/zerobuild-comp.txt ../comp/zerobuild.txt
cp make/make-vm.sh ../vm/make.sh
cp make/zerobuild-nojit-vm.txt ../vm/zerobuild-nojit.txt
cp make/zerobuild-vm.txt ../vm/zerobuild.txt
echo "finished copying makefiles. All OK!"
echo "L1VM - build clang toolchain 1.0"
exit 0
