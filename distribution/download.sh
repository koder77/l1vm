#!/bin/sh
if [ $# -eq 0 ]
  then
    echo "usage: download.sh filename"
	exit 1
fi

# get program obj file and signature
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$1.l1obj.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$1.l1obj.gpg

# get program source code if availaible
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$1.l1com.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$1.l1com.gpg


if ! gpg --verify $1.l1com.gpg $1.l1com.bz2; then
	echo "ERROR: file signature of source code not valid!"
	echo "removing files..."
	rm $1.gpg
	rm $1.l1obj.bz2
fi

# unpack source code and install it
bzip2 -d $1.l1com.bz2
cp $1.l1com ~/l1vm/prog


if ! gpg --verify $1.l1obj.gpg $1.l1obj.bz2; then
	echo "ERROR: file signature of program not valid!"
	echo "removing files..."
	rm $1.gpg
	rm $1.l1obj.bz2
	exit 1
fi

# copy program
cp $1.l1obj.bz2 ~/l1vm/prog

echo "files installed!"
echo "cleaning up..."
rm *.l1com
rm *.bz2
rm *.gpg

exit 0
