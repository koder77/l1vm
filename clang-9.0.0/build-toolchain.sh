#!/bin/bash
echo "L1VM - build clang 9.0.0 toolchain 1.1"
#!get files from llvm.org
echo "downloading archives from llvm.org github..."
wget http://releases.llvm.org/9.0.0/llvm-9.0.0.src.tar.xz
wget http://releases.llvm.org/9.0.0/cfe-9.0.0.src.tar.xz
wget http://releases.llvm.org/9.0.0/llvm-9.0.0.src.tar.xz.sig
wget http://releases.llvm.org/9.0.0/cfe-9.0.0.src.tar.xz.sig
#!get llvm public key
echo "Downloading llvm.org gpg key."
wget http://releases.llvm.org/9.0.0/hans-gpg-key.asc
#!check signed archives
gpg --import hans-gpg-key.asc
if [ $? -gt 0 ]
then
	echo "ERROR: importing pgp key! Stopping!"
	exit 1
fi
echo "The key was successfully added to your keyring!"
gpg --verify cfe-9.0.0.src.tar.xz.sig cfe-9.0.0.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying cfe archive! Stopping!"
	exit 1
fi
gpg --verify llvm-9.0.0.src.tar.xz.sig llvm-9.0.0.src.tar.xz
if [ $? -gt 0 ]
then
	echo "ERROR: verifying llvm archive! Stopping!"
	exit 1
fi

echo "All init stuff done! Let's untar the archives..."
tar -xJf llvm-9.0.0.src.tar.xz

tar -xJf cfe-9.0.0.src.tar.xz -C llvm-9.0.0.src/tools/
mv llvm-9.0.0.src/tools/cfe-9.0.0.src llvm-9.0.0.src/tools/clang

cd llvm-9.0.0.src
mkdir build
cd build
echo "configure..."
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/l1vm-clang-9.0.0 ../
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
echo "L1VM - build clang 9.0.0 toolchain 1.1"
exit 0
