#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!
# Install script for Windows 10 WSL Debian

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

cd assemb
chmod +x *.sh
if ./make.sh; then
	echo "l1asm build ok!"
else
	echo "l1asm build error!"
	exit 1
fi

cd ../comp
chmod +x *.sh
if ./make.sh; then
	echo "l1com build ok!"
else
	echo "l1com build error!"
	exit 1
fi

cd ../vm
chmod +x *.sh
if ./make.sh; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cp l1vm l1vm-jit
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp vm/l1vm-jit ~/bin
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build.sh
if ./install.sh; then
	echo "modules build ok!"
else
	echo "modules build FAILED!"
	exit 1
fi

echo "all modules installed. building programs..."
cd ../prog
chmod +x *.sh
if ./build-all.sh; then
	echo "building programs successfully!"
else
	echo "building programs FAILED!"
	exit 1
fi
cd ..

echo "installation finished!"
exit 0
