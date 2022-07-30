#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "usage: download.sh filename"
	exit 1
fi

# get program name and first char of program name
progname="$1"
progfchar=${progname::1}

# get program obj file and signature
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.l1obj.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.l1obj.gpg

# get program source code if available
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.l1com.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.l1com.gpg

# get readme if available
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.readme.txt.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.readme.txt.gpg

#get data if available
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.data.tar.bz2
curl -O http://midnight-koder.net/blog/assets/l1vm/repo/$progfchar/$progname/$progname.data.tar.gpg

if [ -f $1.l1obj.bz2 ]; then
	if ! gpg --verify $1.l1obj.gpg $1.l1obj.bz2; then
	echo "ERROR: file signature of source code not valid!"
	echo "removing files..."
	rm $1.l1obj.gpg
	rm $1.l1obj.bz2
fi
fi

if [ -f $1.l1com.bz2 ]; then
if ! gpg --verify $1.l1com.gpg $1.l1com.bz2; then
	echo "ERROR: file signature of source code not valid!"
	echo "removing files..."
	rm $1.l1com.gpg
	rm $1.l1com.bz2
fi
fi

if [ -f $1.readme.txt.bz2 ]; then
if ! gpg --verify $1.readme.txt.gpg $1.readme.txt.bz2; then
	echo "ERROR: file signature of readme not valid!"
	echo "removing files..."
	rm $1.readme.txt.gpg
	rm $1.readme.txt.bz2
fi
fi

if [ -f $1.data.tar.bz2 ]; then
if ! gpg --verify $1.data.tar.gpg $1.data.tar.bz2; then
	echo "ERROR: file signature of data not valid!"
	echo "removing files..."
	rm $1.data.tar.gpg
	rm $1.data.tar.bz2
fi
fi


## unpack obj file and install it
bzip2 -d $1.l1obj.bz2
cp $1.l1obj ~/l1vm/prog

// unpack source code and install it
bzip2 -d $1.l1com.bz2
cp $1.l1com ~/l1vm/prog

# unpack readme and install it
bzip2 -d $1.readme.txt.bz2
cp $1.readme.txt ~/l1vm/man

# unpack data and install it
tar -xvjf $1.data.tar.bz2
mkdir ~/l1vm/$1
cp $1.data/* ~/l1vm/$1/ -r

if ! gpg --verify $1.l1obj.gpg $1.l1obj.bz2; then
	echo "ERROR: file signature of program not valid!"
	echo "removing files..."
	rm $1.l1obj.gpg
	rm $1.l1obj.bz2
	exit 1
fi

# copy program
# cp $1.l1obj.bz2 ~/l1vm/prog

echo "files installed!"
echo "cleaning up..."
rm *.l1com
rm *.bz2
rm *.gpg
rm *.txt
rm $1.data -rf
exit 0
