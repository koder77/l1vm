#!/bin/bash
# install modules to /usr/local/lib
numberOfFiles=$(find . -type f | wc -l)
if [ $numberOfFiles != 27 ]
then
	echo "ERROR building modules! Some modules failed to build!"
	exit 1
else
	echo "Building modules was successfull! ALL OK!"
	exit 0
fi
sudo cp libl1vm* /usr/local/lib
