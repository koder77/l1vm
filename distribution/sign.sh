#!/bin/sh
if [ $# -eq 0 ]
  then
    echo "usage: sign.sh filename"
	exit 1
fi

# compress files
bzip2 $1.l1obj
bzip2 $1.l1com

# sign files
gpg --detach-sign --default-key YOURKEYID -o $1.l1obj.gpg $1.l1obj.bz2
gpg --detach-sign --default-key YOURKEYID -o $1.l1com.gpg $1.l1com.bz2
