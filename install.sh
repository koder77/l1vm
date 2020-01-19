#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

echo "checking for needed libraries..."

if ! dpkg -s libsdl1.2-dev &> /dev/null; then
	echo "try to install libsdl1.2-dev..."
	if ! sudo apt-get install libsdl1.2-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl-gfx1.2-dev &> /dev/null; then
	echo "try to install libsdl-gfx1.2-dev..."
	if ! sudo apt-get install libsdl-gfx1.2-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl-image1.2-dev &> /dev/null; then
	echo "try to install libsdl-image1.2-dev..."
	if ! sudo apt-get install libsdl-image1.2-dev; then
		echo "installation failed!"
		exit 1
	fi
fi

if ! dpkg -s libsdl-ttf2.0-dev &> /dev/null; then
	echo "try to install libsdl-ttf2.0-dev..."
	if ! sudo apt-get install libsdl-ttf2.0-dev; then
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

echo "libraries installed! building compiler, assembler and VM..."

cd assemb
if ./make.sh; then
	echo "l1asm build ok!"
else
	echo "l1asm build error!"
	exit 1
fi

cd ../comp
if ./make.sh; then
	echo "l1com build ok!"
else
	echo "l1com build error!"
	exit 1
fi

cd ../vm
if ./make-nojit.sh; then
	echo "l1vm build ok!"
else
	echo "l1vm build error!"
	exit 1
fi
cp l1vm-nojit l1vm
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp vm/l1vm ~/bin
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
./build.sh
if ./install.sh; then
	echo "modules build ok!"
else
	echo "modules build FAILED!"
	exit 1
fi

echo "all modules installed. building programs..."
cd ../prog
if ./build-all.sh; then
	echo "building programs successfully!"
else
	echo "building programs FAILED!"
	exit 1
fi
	cd ..
echo "installation finished!"
exit 0
