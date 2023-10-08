#!/bin/bash
# check if libasmjit is installed
FILE=/usr/local/lib/libasmjit.so
if test -f "$FILE"; then
    echo "$FILE exists!"
else
	echo "libasmjit not installed into $FILE!"
	echo "cloning and building it now..."
	git clone https://github.com/asmjit/asmjit
	cd asmjit
	mkdir build
	cd build
	cmake ../
	make
	sudo make install
	sudo cp libasmjit.so /usr/local/lib
	cd ..
	cd ..
fi
