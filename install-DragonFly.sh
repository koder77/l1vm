#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

if uname -A | grep -q "DragonFly"; then
	echo "DragonFly BSD detected..."
	echo "checking for needed libraries..."
	
	if ! sudo pkg install sdl-1.2.15_14,2; then
		echo "installation of sdl library failed!"
		exit 1
	fi
	
	if ! sudo pkg install sdl_gfx-2.0.25; then
		echo "installation of sdl gfx library failed!"
		exit 1
	fi
		
	if ! sudo pkg install sdl_image-1.2.12_12; then
		echo "installation of sdl image library failed!"
		exit 1
	fi

	if ! sudo pkg install sdl_ttf-2.0.11_7; then
		echo "installation of sdl ttf library failed!"
		exit 1
	fi
	
	if ! sudo pkg install fann-2.2.0; then
		echo "installation of fann library failed!"
		exit 1
	fi
	
	if ! sudo pkg install mpfrc++-3.6.2; then
		echo "installation of mpfrc++ library failed!"
		exit 1
	fi

else
	echo "ERROR: detected OS not DragonFly BSD!"
	echo "EXITING WITH ERROR!"
	exit 1
fi

echo "libraries installed! building compiler, assembler and VM..."

export LIBRARY_PATH=$LIBRARY_PATH:-L/usr/local/lib:-L/usr/lib

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
