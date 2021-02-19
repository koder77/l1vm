#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

if uname -a | grep -q "Debian"; then
echo "Debian detected..."
echo "checking for needed libraries..."

if ! dpkg -s libsdl2-dev &> /dev/null; then
	echo "try to install libsdl2-dev..."
	if ! sudo apt-get install libsdl2-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl2-gfx-dev &> /dev/null; then
	echo "try to install libsdl2-gfx-dev..."
	if ! sudo apt-get install libsdl2-gfx-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl2-image-dev &> /dev/null; then
	echo "try to install libsdl2-image-dev..."
	if ! sudo apt-get install libsdl2-image-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl2-ttf-dev &> /dev/null; then
	echo "try to install libsdl2-ttf-dev..."
	if ! sudo apt-get install libsdl2-ttf-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libfann-dev &> /dev/null; then
	echo "try to install libfann-dev..."
	if ! sudo apt-get install libfann-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libmpfrc++-dev &> /dev/null; then
	echo "try to install libmpfrc++-dev..."
	if ! sudo apt-get install libmpfrc++-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s cmake &> /dev/null; then
	echo "try to install libmpfrc++-dev..."
	if ! sudo apt-get install cmake; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s make &> /dev/null; then
	echo "try to install libmpfrc++-de..."
	if ! sudo apt-get install make; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s git &> /dev/null; then
	echo "try to install git..."
	if ! sudo apt-get install git; then
		echo "installation failed!"
		exit 1
	fi
fi

echo "libraries installed! building compiler, assembler and VM..."

else
	echo "ERROR: detected OS not Debian GNU Linux!"
	echo "You have to install the dependency libraries by hand..."
	echo "See this installation script for more info..."
fi

export CC=clang
export CCPP=clang++

# check if clang C compiler is installed
if ! dpkg -s clang &> /dev/null; then
	echo "try to install clang..."
	if ! sudo apt-get install clang; then
		echo "installation failed!"
		exit 1
	fi
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

# check if zerobuild installed into ~/bin
FILE=~/bin/zerobuild
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "zerobuild not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/koder77/zerobuild.git
	cd zerobuild
	./make.sh
	cp zerobuild ~/bin/
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
	sudo cp mpreal.h /usr/local/include/
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
	sudo make install
	sudo cp libasmjit.so /usr/local/lib
	cd ..
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

cd ../prepro
if zerobuild force; then
	echo "l1pre build ok!"
else
	echo "l1pre build error!"
	exit 1
fi

cd ../vm
if zerobuild force; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp prepro/l1pre ~/bin
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
fi

echo "checking for ~/l1vm directory..."

# check if ~/l1vm exists
DIR="~/l1vm"
if [ -d "$DIR" ]; then
  ### Take action if $DIR exists ###
  echo "${DIR} already exists!"
else
  ###  Control will jump here if $DIR does NOT exists ###
  echo "${DIR} will be created now..."
  mkdir ~/l1vm
fi

cd ..

echo "installing programs to ~/l1vm"
cp prog/ ~/l1vm -r
cp lib/sdl-lib* ~/l1vm/prog

echo "installing lib to ~/lib"
cp lib/ ~/l1vm -r

echo "installing fonts to ~/l1vm"
cp fonts/ ~/l1vm -r

mkdir ~/l1vm/include
cp include-lib/* ~/l1vm/include/

echo "installation finished!"
exit 0
