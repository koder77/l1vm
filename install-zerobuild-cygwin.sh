#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

export PATH="$HOME/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/bin:$LD_LIBRARY_PATH"

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

# install mpreal.h include
	echo "installing mpreal.h include file now..."
	git clone https://github.com/advanpix/mpreal.git
	sudo cp vm/modules/mpfr-c++/mpreal.h /usr/include

cd assemb
chmod +x make-win.sh
if ./make-win.sh; then
	echo "l1asm build ok!"
else
	echo "l1asm build error!"
	exit 1
fi

cd ../comp
chmod +x make-win.sh
if ./make-win.sh; then
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
chmod +x make-win.sh
if ./make-win.sh; then
	echo "l1vm JIT build ok!"
else
	echo "l1vm JIT build error!"
	exit 1
fi
cp l1vm l1vm-nojit
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp prepro/l1pre ~/bin
cp vm/l1vm ~/bin
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

mkdir ~/l1vm/man

echo "copy fann demo neural networks"
cp -R fann ~/l1vm

echo "installation finished!"
exit 0
