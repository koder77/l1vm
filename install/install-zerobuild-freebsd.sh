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

export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

sudo pkg install sdl2
sudo pkg install sdl2_gfx
sudo pkg install sdl2_image
sudo pkg install sdl2_ttf
sudo pkg install sdl2_mixer
sudo pkg install fann
sudo pkg install mpfr
sudo pkg install cmake
sudo pkg install gmake
sudo pkg install git
sudo pkg install libsodium
sudo pkg install libserialport
sudo pkg install pulseaudio
sudo pkg install pavucontrol
sudo pkg install libssl
sudo pkg install libcrypto++

# check if clang C compiler is installed
FILE=/usr/bin/clang
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	sudo pkg install clang
fi

# check if ~/bin exists
DIR="~/l1vm/bin"
if [ -d "$DIR" ]; then
  ### Take action if $DIR exists ###
  echo "${DIR} already exists!"
else
  ###  Control will jump here if $DIR does NOT exists ###
  echo "${DIR} will be created now..."
  mkdir ~/l1vm
  mkdir ~/l1vm/bin
fi

# check if zerobuild installed into ~/bin
FILE=~/l1vm/bin/zerobuild
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "zerobuild not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/koder77/zerobuild.git
	cd zerobuild
	./make.sh
	cp zerobuild ~/l1vm/bin/
	cd ..
fi

# install mpreal.h include
	echo "installing mpreal.h include file now..."
	git clone https://github.com/advanpix/mpreal.git
	sudo cp vm/modules/mpfr-c++/mpreal.h /usr/include

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
if zerobuild zerobuild-nojit.txt force; then
	echo "l1vm build ok!"
else
	echo "l1vm build error!"
	exit 1
fi
cd ..
cp assemb/l1asm ~/l1vm/bin
cp comp/l1com ~/l1vm/bin
cp prepro/l1pre ~/l1vm/bin
cp vm/l1v* ~/l1vm/bin
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
sh ./build-freebsd.sh
if sh ./install-openbsd.sh; then
	echo "modules build ok!"
else
	echo "modules build FAILED!"
	exit 1
fi

cd ../

echo "all modules installed. building programs..."
chmod +x *.sh
if sh ./build-all.sh; then
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

echo "installation finished!"

echo "The installation guide is here: https://midnight-coding.de/blog/software/l1vm/2025/11/30/L1VM-install-guide.html"
echo "You need to set ENV variables to make L1VM working!"
exit 0
