#!/bin/bash
# install modules to /usr/local/lib
numberOfFiles=$(find . -type f | wc -l)
if [ $numberOfFiles != 18 ]
then
	echo "ERROR building modules! Some modules failed to build!"
	exit 1
else
	echo "Building modules was successfull! Installing..."
	cp libl1vm* ~/bin
	exit 0
fi
