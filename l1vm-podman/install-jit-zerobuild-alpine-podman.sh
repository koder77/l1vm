#!/bin/bash
#

echo "installing needed packages..."
apk update
apk add clang
apk add cmake
apk add --upgrade fann
apk add --upgrade fann-dev
apk add --upgrade fann-float
apk add --upgrade fann-double
apk add --upgrade mpfr
apk add --upgrade mpfr-dev
apk add --upgrade libc-dev
apk add --upgrade git
apk add --upgrade make automake
apk add --upgrade binutils
apk add --upgrade gcc
apk add --upgrade gcompat
apk add --upgrade libstdc++
apk add --upgrade g++
apk add --upgrade gmp
apk add --upgrade gmp-dev

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

# check if zerobuild installed into /usr/local/bin
FILE=/usr/local/bin/zerobuild
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "zerobuild not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/koder77/zerobuild.git
	cd zerobuild
	./make.sh
	cp zerobuild /usr/local/bin/
	cd ..
fi

# check if mpreal.h is installed
FILE=/usr/include/mpreal.h
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "mpreal.h not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/advanpix/mpreal.git
	cd mpreal
	cp mpreal.h /usr/include
	cd ..
fi

cd assemb
if zerobuild force; then
	echo "l1asm build ok!"
else
	echo "l1asm build error!"
	exit 1
fi

cd ../comp
if zerobuild force; then
	echo "l1com build ok!"
else
	echo "l1com build error!"
	exit 1
fi

cd ../vm
if ./make-cli.sh; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cp l1vm-cli l1vm-jit
cd ..
cp assemb/l1asm /usr/local/bin/
cp comp/l1com /usr/local/bin/
cp vm/l1vm-jit /usr/local/bin/
echo "VM binaries installed into /usr/local/bin/"

cd modules
echo "installing modules..."
chmod +x *.sh
sh ./build-alpine.sh
sh ./install-alpine.sh

echo "all modules installed. building programs..."
cd ../prog
chmod +x *.sh
if sh ./build-all.sh; then
	echo "building programs successfully!"
else
	echo "building programs FAILED!"
	exit 1
fi
cd ..

echo "installation finished!"
exit 0