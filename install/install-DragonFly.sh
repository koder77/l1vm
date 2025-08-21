#!/bin/bash
# changed: install to /home/foo/bin instead to /usr/local/bin!

cd ..

if uname -a | grep -q "DragonFly"; then
	echo "DragonFly BSD detected..."
	echo "checking for needed libraries..."

	if ! sudo pkg install sdl2; then
		echo "installation of sdl library failed!"
		exit 1
	fi

	if ! sudo pkg install sdl2_gfx; then
		echo "installation of sdl gfx library failed!"
		exit 1
	fi

	if ! sudo pkg install sdl2_image; then
		echo "installation of sdl image library failed!"
		exit 1
	fi

	if ! sudo pkg install sdl2_ttf; then
		echo "installation of sdl ttf library failed!"
		exit 1
	fi

	if ! sudo pkg install sdl2_mixer; then
		echo "installation of sdl mixer library failed!"
		exit 1
	fi

	if ! sudo pkg install fann; then
		echo "installation of fann library failed!"
		exit 1
	fi

	if ! sudo pkg install mpfrc++; then
		echo "installation of mpfrc++ library failed!"
		exit 1
	fi

	if ! sudo pkg install cmake; then
		echo "installation of cmake failed!"
		exit 1
	fi

	if ! sudo pkg install gmake; then
		echo "installation of gmake failed!"
		exit 1
	fi

	if ! sudo pkg install git; then
		echo "installation of git failed!"
		exit 1
	fi

	if ! sudo pkg install libsodium; then
		echo "installation of sodium library failed!"
		exit 1
	fi

	if ! sudo pkg install libserialport; then
		echo "installation of serialport library failed!"
		exit 1
	fi

	if ! sudo pkg install libssl; then
		echo "installation of serialport library failed!"
		exit 1
	fi

	if ! sudo pkg install libcrypto++; then
		echo "installation of libcrypto library failed!"
		exit 1
	fi

else
	echo "ERROR: detected OS not DragonFly BSD!"
	echo "EXITING WITH ERROR!"
	exit 1
fi

echo "libraries installed! building compiler, assembler and VM..."

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/usr/lib

export CC=/usr/local/llvm10/bin/clang
export CCPP=/usr/local/llvm10/bin/clang++

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
cp vm/l1v* ~/bin
echo "VM binaries installed into ~/bin"

cd modules
echo "installing modules..."
chmod +x *.sh
./build-df.sh
if ./install-df.sh; then
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
	exit 1
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
exit 0
