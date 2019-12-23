#!/bin/bash
cd assemb
./make.sh
cd ../comp
./make.sh
cd ../vm 
./make-nojit.sh
cp l1vm-nojit l1vm
cd ..
sudo cp assemb/l1asm /usr/local/bin
sudo cp comp/l1com /usr/local/bin
sudo cp vm/l1vm /usr/local/bin
echo "VM binaries installed into /usr/local/bin"
cd modules
echo "installing modules..."
./build.sh
./install.sh
echo "all modules installed. installation finished"
cd ..
