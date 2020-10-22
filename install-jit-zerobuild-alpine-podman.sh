#!/bin/bash
#

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
FILE=/usr/local/include/mpreal.h
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "mpreal.h not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/advanpix/mpreal.git
	cd mpreal
	cp mpreal.h /usr/local/include
	cd ..
fi

# check if libasmjit is installed
FILE=/usr/local/lib/libasmjit.so
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "libasmjit not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/asmjit/asmjit
	cd asmjit
	mkdir build
	cd build
	cmake ../
	make
	make install
	cp libasmjit.so /usr/local/lib
	cd ..
	cd ..
fi

# check if libl1vm-jit.so is installed
FILE=/usr/local/lib/libl1vm-jit.so
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "libl1vm-jit not installed into $FILE!"
	echo "building it now..."
	cd libjit
	zerobuild force
	cp libl1vm-jit.so /usr/local/lib
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
if ./make-cli-jit.sh; then
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
sh ./build.sh
sh ./install.sh

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
