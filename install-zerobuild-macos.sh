#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!
#

export PATH="$HOME/bin:$PATH"
export DYLD_LIBRARY_PATH="$HOME/bin:$DYLD_LIBRARY_PATH"

#install brew
xcode-select --install
# find / -name features.h

cp /Library/Developer/CommandLineTools/SDKs/MacOSX11.3.sdk/usr/include/machine/endian.h /usr/local/include
cp /usr/local/include/c++/9.4.0/parallel/features.h /usr/local/include

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# install needed libraries
brew install sdl2
brew install sdl2_gfx
brew install sdl2_image
brew install sdl2_ttf
brew install sdl2_mixer
brew install fann
brew install mpfr
brew install libsodium
brew install cmake
brew install make
brew install git

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

echo "mpreal.h installation"
echo "cloning and building it now..."
git clone https://github.com/advanpix/mpreal.git
# cd mpreal
# sudo cp mpreal.h /usr/local/include
cp vm/modules/mpfr-c++/mpreal.h /usr/local/include
cd ..

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
if zerobuild zerobuild-nojit-macos.txt force; then
	echo "l1vm build ok!"
else
	echo "l1vm build error!"
	exit 1
fi
cd ..
cp assemb/l1asm ~/bin
cp comp/l1com ~/bin
cp prepro/l1pre ~/bin
cp vm/l1vm-nojit ~/bin/l1vm
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build-macos.sh
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
cd ..

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

echo "installation finished!"
exit 0
