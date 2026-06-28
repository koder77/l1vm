#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

echo "Install base package with includes and programs..."
# Known-good SHA256 checksum for l1vm-base-pkg.tar.bz2 - update when the package is updated
EXPECTED_SHA256="b0bc1a8d684473f6b013b8a31aa25d46ee5c5c78db08a7bbd164e1357621ced4"
../scripts/download-pkg.sh https://midnight-coding.de/blog/assets/l1vm/l1vm-base-pkg.tar.bz2 l1vm-base-pkg.tar.bz2
ACTUAL_SHA256=$(sha256 -q l1vm-base-pkg.tar.bz2 2>/dev/null || openssl dgst -sha256 l1vm-base-pkg.tar.bz2 2>/dev/null | awk '{print $NF}')
if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
	echo "ERROR: Checksum verification FAILED for l1vm-base-pkg.tar.bz2!"
	echo "Expected: $EXPECTED_SHA256"
	echo "Actual:   $ACTUAL_SHA256"
	echo "The download may be corrupted or tampered with. Aborting installation."
	rm -f l1vm-base-pkg.tar.bz2
	exit 1
fi
echo "Checksum verified OK."

cd ..

echo "building compiler, assembler and VM..."

# use GCC, because clang can't build some modules!
export CC=gcc
export CCPP=g++

pkgman install llvm12_clang
pkgman install libsdl2_devel
pkgman install sdl2_gfx_devel
pkgman install sdl2_image_devel
pkgman install sdl2_mixer_devel
pkgman install sdl2_ttf_devel
pkgman install mpfr_devel
pkgman install cmake
pkgman install make
pkgman install git
pkgman install libsodium_devel
pkgman install libssl_devel
pkgman install libcrypto++_devel

# check if zerobuild installed into /boot/home/config/non-packaged/bin/
FILE=/boot/home/config/non-packaged/bin/zerobuild
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "zerobuild not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/koder77/zerobuild.git
	cd zerobuild
	./make.sh
	cp zerobuild /boot/home/config/non-packaged/bin
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

export CC=clang
export CCPP=clang++

cd ../vm
if zerobuild zerobuild-nojit-haiku.txt force; then
	echo "l1vm build ok!"
else
	echo "l1vm build error!"
	exit 1
fi
cp l1vm-nojit l1vm
cd ..
cp assemb/l1asm /boot/home/config/non-packaged/bin
cp comp/l1com /boot/home/config/non-packaged/bin
cp prepro/l1pre /boot/home/config/non-packaged/bin
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

echo "installation finishedi!"

echo "The installation guide is here: https://midnight-coding.de/blog/software/l1vm/2025/11/30/L1VM-install-guide.html"
echo "You need to set ENV variables to make L1VM working!"
exit 0
