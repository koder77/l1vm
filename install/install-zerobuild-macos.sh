#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!
#

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
export DYLD_LIBRARY_PATH="$HOME/l1vm/bin:$DYLD_LIBRARY_PATH"

#install brew
xcode-select --install
#find / -name floatfann.h
# fsexit 0

# macOS 13 build
cp /Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk/usr/include/machine/endian.h /usr/local/include
cp /usr/local/include/c++/11/parallel/features.h /usr/local/include

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

echo "SDK INCLUDE PATH:"
xcrun --show-sdk-path


#install clang
brew update
brew upgrade
brew install llvm

# install needed libraries
brew install sdl2
brew install sdl2_gfx
brew install sdl2_image
brew install sdl2_ttf
brew install sdl2_mixer
brew install fann
brew install mpfr
brew install libsodium
brew install libserialport
brew install openssl
brew install cmake
brew install make
brew install git


#find / -name stdio.h
#echo "SDL include search:"
#find / -name endian.h
#exit 0


clang --version
echo "building compiler, assembler and VM..."

export CC=clang
export CCPP=clang++

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

echo "mpreal.h installation"
echo "cloning and building it now..."
git clone https://github.com/advanpix/mpreal.git
sudo cp vm/modules/mpfr-c++/mpreal.h /usr/local/include

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
cp assemb/l1asm ~/l1vm/bin
cp comp/l1com ~/l1vm/bin
cp prepro/l1pre ~/l1vm/bin
cp vm/l1vm-nojit ~/l1vm/bin/l1vm
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build-macos.sh
if ./install-macos.sh; then
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

echo "installation finished!"

echo "The installation guide is here: https://midnight-coding.de/blog/software/l1vm/2025/11/30/L1VM-install-guide.html"
echo "You need to set ENV variables to make L1VM working!"
exit 0
