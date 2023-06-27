#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

echo "building compiler, assembler and VM..."

# use GCC, because clang can't build some modules!
export CC=clang
export CCPP=clang++

pkgman install libsdl2_devel
pkgman install sdl2_gfx_devel
pkgman install sdl2_image_devel
pkgman install sdl2_mixer_devel
pkgman install sdl2_ttf_devel
pkgman install mpfr_devel
pkgman install CMake
pkgman install make
pkgman install Git
pkgman install libsodium

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
if zerobuild zerobuild-nojit-haiku.txt force; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cp l1vm-nojit l1vm
cd ..
cp assemb/l1asm /boot/home/config/non-packaged/bin
cp comp/l1com /boot/home/config/non-packaged/bin
cp vm/l1v* /boot/home/config/non-packaged/bin
echo "VM binaries installed into /boot/home/config/non-packaged/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build-haiku.sh
if ./install-haiku.sh; then
	echo "modules build ok!"
else
	echo "modules build FAILED!"
	exit 1
fi

cd ../

echo "all modules installed. building programs..."
chmod +x *.sh
if ./build-all.sh; then
	echo "building programs successfully!"
else
	echo "building programs FAILED!"
fi

mkdir ~/l1vm

echo "installing programs to ~/l1vm"
cp prog/ ~/l1vm -r
cp lib/sdl-lib* ~/l1vm/prog

echo "installing lib to ~/lib"
cp lib/ ~/l1vm -r

echo "installing fonts to ~/l1vm"
cp fonts/ ~/l1vm -r

mkdir ~/l1vm/include
cp include-lib/* ~/l1vm/include/

mkdir ~/l1vm/man

echo "copy fann demo neural networks"
cp -R fann ~/l1vm

echo "installation finished!"
exit 0
