#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

pacman -S mingw-w64-x86_64-clang --noconfirm
pacman -S git --noconfirm
pacman -S cmake --noconfirm

pacman -S mingw-w64-x86_64-SDL2 --noconfirm
pacman -S mingw-w64-x86_64-SDL2_gfx --noconfirm
pacman -S mingw-w64-x86_64-SDL2_ttf --noconfirm
pacman -S mingw-w64-x86_64-SDL2_image --noconfirm
pacman -S mingw-w64-x86_64-SDL2_mixer --noconfirm

pacman -S mingw-w64-x86_64-mpfr --noconfirm
pacman -S mingw-w64-x86_64-libsodium --noconfirm
pacman -S mingw-w64-x86_64-libserialport --noconfirm

echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

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
cp l1vm l1vm-nojit
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp prepro/l1pre ~/bin
cp vm/l1v* ~/bin
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build-win.sh
if ./install-win.sh; then
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
cp prog/ ~/l1vm -r
cp lib/sdl-lib* ~/l1vm/prog

echo "installing lib to ~/lib"
cp lib/ ~/l1vm -r

echo "installing fonts to ~/l1vm"
cp fonts/ ~/l1vm -r

cp include-lib/ ~/l1vm/include -r

mkdir ~/l1vm/man

echo "copy fann demo neural networks"
cp -R fann ~/l1vm

echo "installation finished!"
exit 0
