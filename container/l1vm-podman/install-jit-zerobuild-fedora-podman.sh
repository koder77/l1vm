#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

sudo dnf install SDL2-devel.x86_64
sudo dnf install SDL2_gfx-devel.x86_64
sudo dnf install SDL2_image-devel.x86_64
sudo dnf install SDL2_ttf-devel.x86_64
sudo dnf install fann-devel.x86_64
sudo dnf install mpfr-devel.x86_64
sudo dnf install clang.x86_64
sudo dnf install git
sudo dnf install clang
sudo dnf install make cmake
sudo dnf install nano
sudo dnf install findutils

# check if clang C compiler is installed
FILE=/usr/bin/clang
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	sudo dnf install clang
fi

# check if ~/bin exists
DIR="~/bin"
if [ -d "$DIR" ]; then
  ### Take action if $DIR exists ###
  echo "${DIR} already exists!"
else
  ###  Control will jump here if $DIR does NOT exists ###
  echo "${DIR} will be created now..."
  mkdir ~/bin
fi

# check if zerobuild installed into /usr/bin
FILE=/usr/bin/zerobuild
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "zerobuild not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/koder77/zerobuild.git
	cd zerobuild
	./make.sh
	sudo cp zerobuild /usr/bin
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
	sudo cp mpreal.h /usr/local/include
	cd ..
fi

# check if libasmjit is installed
FILE=/usr/lib/libasmjit.so
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
	sudo make install
	sudo cp libasmjit.so /usr/lib
	cd ..
	cd ..
fi

# check if libl1vm-jit.so is installed
FILE=/usr/lib/libl1vm-jit.so
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "libl1vm-jit not installed into $FILE!"
	echo "building it now..."
	cd libjit
	zerobuild force
	sudo cp libl1vm-jit.so /usr/lib
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
if zerobuild force; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cp l1vm l1vm-jit
cd ..
sudo cp assemb/l1asm /usr/bin
sudo cp comp/l1com /usr/bin
sudo cp vm/l1vm-jit /usr/bin
echo "VM binaries installed into /usr/bin"

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
