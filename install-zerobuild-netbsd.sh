#!/bin/bash
# This is experimental amd may not work right!

export PATH="$HOME/l1vm/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/l1vm/bin:$LD_LIBRARY_PATH"

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

sudo pkgin -y install SDL2
sudo pkgin -y install SDL2_gfx
sudo pkgin -y install SDL2_image
sudo pkgin -y install SDL2_ttf
sudo pkgin -y install SDL2_mixer
sudo pkgin -y install fann
sudo pkgin -y install mpfr
sudo pkgin -y install cmake
sudo pkgin -y install gmake
sudo pkgin -y install git
sudo pkgin -y install libsodium
sudo pkgin -y install libserialport
sudo pkgin -y install pulseaudio
sudo pkgin -y install pavucontrol
sudo pkgin -y install libssl
sudo pkgin -y install libcrypto++

# check if clang C compiler is installed
FILE=/usr/bin/clang
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	sudo pkgin -y install clang
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
	GIT_SSL_NO_VERIFY=true git clone https://github.com/advanpix/mpreal.git
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
if zerobuild zerobuild-nojit-netbsd.txt force; then
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
sh ./build-netbsd.sh
if sh ./install.sh; then
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

echo "installing programs to ~/l1vm"
cp -R prog/ ~/l1vm
cp lib/sdl-lib* ~/l1vm/prog

echo "installing lib to ~/lib"
cp -R lib/ ~/l1vm

echo "installing fonts to ~/l1vm"
cp -R fonts/ ~/l1vm

mkdir ~/l1vm/include
cp include-lib/* ~/l1vm/include/

mkdir ~/l1vm/man

echo "copy fann demo neural networks"
cp -R fann ~/l1vm

echo "installation finished!"
exit 0
