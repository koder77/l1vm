#!/bin/bash
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
sudo cp assemb/l1asm /usr/local/bin
sudo cp comp/l1com /usr/local/bin
sudo cp vm/l1vm /usr/local/bin
echo "VM binaries installed into /usr/local/bin"
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
