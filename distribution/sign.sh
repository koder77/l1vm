#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "usage: sign.sh filename"
	exit 1
fi

# compress files
bzip2 $1.l1obj
bzip2 $1.l1com
bzip2 $1.readme.txt
tar -cjf $1.data.tar.bz2 $1.data

# sign files
gpg --detach-sign --default-key YOURKEYID -o $1.l1obj.gpg $1.l1obj.bz2
gpg --detach-sign --default-key YOURKEYID -o $1.l1com.gpg $1.l1com.bz2
gpg --detach-sign --default-key YOURKEYID -o $1.readme.txt.gpg $1.readme.txt.bz2
gpg --detach-sign --default-key YOURKEYID -o $1.data.tar.gpg $1.data.tar.bz2
